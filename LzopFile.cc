#include "LzopFile.h"

#include <algorithm>
#include <stdexcept>
#include <fcntl.h>

#include <libkern/OSByteOrder.h>

#include <lzo/lzo1x.h>

const char LzopFile::Magic[] = { 0x89, 'L', 'Z', 'O', '\0', '\r', '\n',
	0x1a, '\n' };

void LzopFile::parseHeader(FileHandle& fh, uint32_t& flags) {
	// Check magic
	Buffer magic;
	fh.read(magic, sizeof(Magic));
	if (memcmp(&magic[0], Magic, magic.size()) != 0)
		throw std::runtime_error("magic mismatch");
	
	// FIXME: Check headers for sanity
	// skip versions, method, level
	fh.seek(3 * sizeof(uint16_t) + 2 * sizeof(uint8_t), SEEK_CUR);
	fh.readBE(flags);
	// skip mode, mtimes, filename, checksum
	fh.seek(3 * sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint32_t),
		SEEK_CUR);
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
		fh.seek(sums + csize, SEEK_CUR);
	}
}

std::string LzopFile::indexPath() const {
	return mPath + ".blockIdx";
}

namespace {
	bool gLzopInited = false;
}

LzopFile::LzopFile(const std::string& path) 
		: mPath(path) {
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

LzopFile::BlockIterator LzopFile::findBlock(off_t off) const {
	BlockIterator iter = std::lower_bound(
		mBlocks.begin(), mBlocks.end(), off, BlockOffsetOrdering());
	if (iter == mBlocks.end())
		throw std::runtime_error("can't find block");
	return iter;
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
	fprintf(stderr, "Decompressing from %lld\n", b.coff);
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
