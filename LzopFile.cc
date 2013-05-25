#include "LzopFile.h"

#include "PathUtils.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <stdexcept>

#include <inttypes.h>

#include <lzo/lzo1x.h>

const unsigned char LzopFile::Magic[] = { 0x89, 'L', 'Z', 'O', '\0', '\r', '\n',
	0x1a, '\n' };

// Version of lzop we emulate
const uint16_t LzopFile::LzopDecodeVersion = 0x1010;

void LzopFile::readHeaders(FileHandle& fh, uint32_t& flags) {
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

void LzopFile::buildIndex(FileHandle& fh) {
	off_t size = fh.size();
	
	uint32_t flags = mFlags;
	off_t uoff = 0;
	while (true) {
		uoff = findBlocks(fh, flags, uoff);
		if (fh.tell() >= size)
			break;
		readHeaders(fh, flags);
	}
}

off_t LzopFile::findBlocks(FileHandle& fh, uint32_t flags, off_t uoff) {	
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
	off_t coff = fh.tell();
	size_t sums;
	while (true) {
		fh.readBE(usize);
		if (usize == 0)
			return uoff;
		fh.readBE(csize);
		
		sums = usums;
		if (usize != csize)
			sums += csums;
		
		addBlock(new Block(usize, csize, uoff, coff + bheader + sums));
		
		coff += sums + csize + 2 * sizeof(uint32_t);
		uoff += usize;
		fh.seek(sums + csize);
	}
}

namespace {
	bool gLzopInited = false;
}

LzopFile::LzopFile(const std::string& path, uint64_t maxBlock)
		: IndexedCompFile(path) {
	if (!gLzopInited) {
		lzo_init();
		gLzopInited = true;
	}
	initialize(maxBlock);
}

void LzopFile::decompressBlock(const FileHandle& fh, const Block& b,
		Buffer& ubuf) const {
	if (b.csize == b.usize) { // Uncompressed, just read it
		fh.pread(b.coff, ubuf, b.usize);
		return;
	}
	
	Buffer cbuf;
	fh.pread(b.coff, cbuf, b.csize);	
	
	ubuf.resize(b.usize);
	lzo_uint usize = b.usize;
//	fprintf(stderr, "Decompressing from %" PRIu64 "\n", uint64_t(b.coff));
	int err = lzo1x_decompress_safe(&cbuf[0], cbuf.size(), &ubuf[0],
		&usize, 0);
	if (err != LZO_E_OK) {
//		fprintf(stderr, "lzo err: %d\n", err);
		throw std::runtime_error("decompression error");
	}
}

std::string LzopFile::destName() const {
	using namespace PathUtils;
	std::string base = basename(path());
	if (replaceExtension(base, "tzo", "tar")) return base;
	if (removeExtension(base, "lzo")) return base;
	return base;
}
