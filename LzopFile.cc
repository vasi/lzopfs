#include "LzopFile.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <stdexcept>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <lzo/lzo1x.h>

const char LzopFile::Magic[] = { 0x89, 'L', 'Z', 'O', '\0', '\r', '\n',
	0x1a, '\n' };

// Version of lzop we emulate
const uint16_t LzopFile::LzopDecodeVersion = 0x1010;

void LzopFile::parseHeader(FileHandle& fh, uint32_t& flags) {
	try {		
		// Check magic
		Buffer magic;
		fh.read(magic, sizeof(Magic));
		if (memcmp(&magic[0], Magic, magic.size()) != 0)
			throwFormat("magic mismatch");
		off_t headerStart = fh.tell();
		
		uint16_t lzopEncVers, lzopMinVers, lzoVers;
		fh.readBE(lzopEncVers);
		fh.readBE(lzoVers);
		fh.readBE(lzopMinVers);
		if (lzopMinVers > LzopDecodeVersion)
			throwFormat("lzop version too new");
		
		uint8_t method, level;
		fh.readBE(method);
		fh.readBE(level);
		
		fh.readBE(flags);
		if (flags & MultiPart)
			throwFormat("multi-part archives not supported");
		if (flags & Filter)
			throwFormat("filter not supported");
		
		fh.seek(3 * sizeof(uint32_t)); // skip mode, mtimes
		
		uint8_t filenameSize;
		fh.readBE(filenameSize);
		if (filenameSize > 0)
			fh.seek(filenameSize);
		
		
		// Check the checksum
		size_t headerSize = fh.tell() - headerStart;
		fh.seek(headerStart, SEEK_SET);
		Buffer header;
		fh.read(header, headerSize);
		
		Checksum cksum;
		fh.readBE(cksum);
		if (cksum != checksum((flags & HeaderCRC) ? CRC : Adler, header))
			throwFormat("checksum mismatch");
		
		
		if (flags & ExtraField) { // unused?
			uint32_t extraSize;
			fh.readBE(extraSize);
			fh.seek(extraSize + sizeof(Checksum));
		}		
	} catch (FileHandle::EOFException& e) {
		throwFormat("EOF");
	}
}

LzopFile::Checksum LzopFile::checksum(ChecksumType type, const Buffer& buf) {
	lzo_uint32 init = (type == CRC) ? 0 : 1;
	return (type == CRC ? lzo_crc32 : lzo_adler32)(init, &buf[0], buf.size());
}

void LzopFile::parseBlocks() {
	uint32_t flags;
	FileHandle fh(mPath, O_RDONLY);
	parseHeader(fh, flags);
	
	// How much space for checksums?
	size_t csums = 0, usums = 0;
	if (flags & CRCComp) ++csums;
	if (flags & AdlerComp) ++csums;
	if (flags & CRCDec) ++usums;
	if (flags & AdlerDec) ++usums;
	csums *= sizeof(uint32_t);
	usums *= sizeof(uint32_t);
	
	// Iterate thru the blocks
	size_t bheader = 2 * sizeof(uint32_t);
	uint32_t usize, csize;
	off_t uoff = 0, coff = fh.tell();
	size_t sums;
	while (true) {
		fh.readBE(usize);
		if (usize == 0)
			break;
		fh.readBE(csize);
		
		sums = usums;
		if (usize != csize)
			sums += csums;
		
		mBlocks.push_back(Block(usize, csize,
			coff + bheader + sums, uoff));
		
		coff += sums + csize + 2 * sizeof(uint32_t);
		uoff += usize;
		fh.seek(sums + csize);
	}
}

std::string LzopFile::indexPath() const {
	return mPath + ".blockIdx";
}

namespace {
	bool gLzopInited = false;
}

LzopFile::LzopFile(const std::string& path) : CompressedFile(path) {
	if (!gLzopInited) {
		lzo_init();
		gLzopInited = true;
	}
	
	if (!readIndex()) {
		parseBlocks();
		writeIndex();
	}
	fprintf(stderr, "Ready\n");
}

// True on success
bool LzopFile::readIndex() {
	FileHandle fh;
	try {
		fh.open(indexPath(), O_RDONLY);
	} catch (FileHandle::Exception& e) {
		return false;
	}
	
	uint32_t usize, csize;
	uint64_t uoff = 0, coff;
	while (true) {
		fh.readBE(usize);
		if (usize == 0)
			return true;
		fh.readBE(csize);
		fh.readBE(coff);
		
		mBlocks.push_back(Block(usize, csize, coff, uoff));
		uoff += usize;
	}
}

void LzopFile::writeIndex() const {
	FileHandle fh(indexPath(), O_WRONLY | O_CREAT, 0664);
	for (BlockList::const_iterator iter = mBlocks.begin();
			iter != mBlocks.end(); ++iter) {
		fh.writeBE(iter->usize);
		fh.writeBE(iter->csize);
		fh.writeBE(iter->coff);
	}
	uint32_t eof = 0;
	fh.writeBE(eof);
	fprintf(stderr, "Wrote index\n");
}

namespace {
struct BlockOffsetOrdering {
	bool operator()(const Block& b, off_t off) {
		return (b.uoff + b.usize - 1) < (uint64_t)off;
	}
	
	bool operator()(off_t off, const Block& b) {
		return (uint64_t)off < b.uoff;
	}
};
}

CompressedFile::BlockIterator LzopFile::findBlock(off_t off) const {
	BlockList::const_iterator iter = std::lower_bound(
		mBlocks.begin(), mBlocks.end(), off, BlockOffsetOrdering());
	if (iter == mBlocks.end())
		throw std::runtime_error("can't find block");
	return BlockIterator(new Iterator(iter, mBlocks.end()));
}

void LzopFile::decompressBlock(FileHandle& fh, const Block& b,
		Buffer& ubuf) {
	fh.seek(b.coff, SEEK_SET);	
	if (b.csize == b.usize) { // Uncompressed, just read it
		fh.read(ubuf, b.usize);
		return;
	}
	
	Buffer cbuf;
	fh.read(cbuf, b.csize);	
	
	ubuf.resize(b.usize);
	lzo_uint usize = b.usize;
	fprintf(stderr, "Decompressing from %" PRIu64 "\n", uint64_t(b.coff));
	int err = lzo1x_decompress_safe(&cbuf[0], cbuf.size(), &ubuf[0],
		&usize, 0);
	if (err != LZO_E_OK) {
		fprintf(stderr, "lzo err: %d\n", err);
		throw std::runtime_error("decompression error");
	}
}

off_t LzopFile::uncompressedSize() const {
	if (mBlocks.empty())
		return 0;
	const Block& b = mBlocks.back();
	return b.uoff + b.usize;
}
