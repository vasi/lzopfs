#include "LzopFile.h"

#include <algorithm>
#include <stdexcept>
#include <fcntl.h>

#include <libkern/OSByteOrder.h>

#include <lzo/lzo1x.h>

const char LzopFile::Magic[] = { 0x89, 'L', 'Z', 'O', '\0', '\r', '\n',
	0x1a, '\n' };

// File position must be at the start of the first block
void LzopFile::parseBlocks() {
	// How much space for checksums?
	size_t csums = 0, usums = 0;
	if (mFlags & CRCComp) ++csums;
	if (mFlags & AdlerComp) ++csums;
	if (mFlags & CRCDec) ++usums;
	if (mFlags & AdlerDec) ++usums;
	csums *= sizeof(uint32_t);
	usums *= sizeof(uint32_t);
	
	// Iterate thru the blocks
	size_t bheader = 2 * sizeof(uint32_t);
	uint32_t usize, csize;
	off_t uoff = 0, coff = tell();
	size_t sums;
	while (true) {
		readBE(usize);
		if (usize == 0)
			break;
		readBE(csize);
		
		sums = usums;
		if (usize != csize)
			sums += csums;
		
		mBlocks.push_back(Block(usize, csize,
			coff + bheader + sums, uoff));
		
		coff += sums + csize + 2 * sizeof(uint32_t);
		uoff += usize;
		skip(sums + csize);
	}
}

template <typename T>
void LzopFile::convertBE(T &t) {
	#ifdef __LITTLE_ENDIAN__
		char *p = reinterpret_cast<char*>(&t);
		std::reverse(p, p + sizeof(T));
	#endif
}

template <typename T>
void LzopFile::readBE(T& t) {
	read(&t, sizeof(T));
	convertBE(t);
}

void LzopFile::read(void *buf, size_t size) {
	ssize_t bytes = ::read(mFD, buf, size);
	if (bytes < 0 || (size_t)bytes != size)
		throw std::runtime_error("read failure");
}

off_t LzopFile::seek(off_t off, int whence) {
	off_t ret = lseek(mFD, off, whence);
	if (ret == -1)
		throw std::runtime_error("seek failure");
	return ret;
}

void LzopFile::seek(off_t off) {
	seek(off, SEEK_SET);
}

void LzopFile::skip(off_t off) {
	seek(off, SEEK_CUR);
}

off_t LzopFile::tell() {
	return seek(0, SEEK_CUR);
}

namespace {
	bool gLzopInited = false;
}

LzopFile::LzopFile(const char *path) {
	if (!gLzopInited) {
		lzo_init();
		gLzopInited = true;
	}
	
	mFD = open(path, O_RDONLY);
	
	// Check magic
	char magic[sizeof(Magic)];
	read(magic, sizeof(magic));
	if (memcmp(magic, Magic, sizeof(magic)) != 0)
		throw std::runtime_error("magic mismatch");
	
	// FIXME: Check headers for sanity
	// skip versions, method, level
	skip(3 * sizeof(uint16_t) + 2 * sizeof(uint8_t));
	readBE(mFlags);
	// skip mode, mtimes, filename, checksum
	skip(3 * sizeof(uint32_t) + sizeof(uint8_t)
		+ sizeof(uint32_t));
	
	parseBlocks();
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

void LzopFile::decompressBlock(const Block& b, Buffer& cbuf, Buffer& ubuf) {
	seek(b.coff);
	cbuf.resize(b.csize);
	read(&cbuf[0], b.csize);
	
	lzo_uint usize = b.usize;
	ubuf.resize(usize);
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
