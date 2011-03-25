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
	off_t uoff = 0, coff = seek(0, SEEK_CUR); // tell
	size_t sums;
	while (true) {
		readBE(mFD, usize);
		if (usize == 0)
			break;
		readBE(mFD, csize);
		
		sums = usums;
		if (usize != csize)
			sums += csums;
		
		mBlocks.push_back(Block(usize, csize,
			coff + bheader + sums, uoff));
		
		coff += sums + csize + 2 * sizeof(uint32_t);
		uoff += usize;
		seek(sums + csize, SEEK_CUR);
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
void LzopFile::readBE(int fd, T& t) {
	read(fd, &t, sizeof(T));
	convertBE(t);
}

template <typename T>
void LzopFile::writeBE(int fd, T t) {
	convertBE(t);
	write(fd, &t, sizeof(T));
}

void LzopFile::read(int fd, void *buf, size_t size) {
	ssize_t bytes = ::read(fd, buf, size);
	if (bytes < 0 || (size_t)bytes != size)
		throw std::runtime_error("read failure");
}

off_t LzopFile::seek(off_t off, int whence) {
	off_t ret = lseek(mFD, off, whence);
	if (ret == -1)
		throw std::runtime_error("seek failure");
	return ret;
}

std::string LzopFile::indexPath() const {
	return mPath + ".blockIdx";
}

namespace {
	bool gLzopInited = false;
}

LzopFile::LzopFile(const std::string& path, MissingIndexBehavior mib) 
		: mPath(path) {
	if (!gLzopInited) {
		lzo_init();
		gLzopInited = true;
	}
	
	mFD = open(path.c_str(), O_RDONLY);
	
	// Check magic
	char magic[sizeof(Magic)];
	read(mFD, magic, sizeof(magic));
	if (memcmp(magic, Magic, sizeof(magic)) != 0)
		throw std::runtime_error("magic mismatch");
	
	// FIXME: Check headers for sanity
	// skip versions, method, level
	seek(3 * sizeof(uint16_t) + 2 * sizeof(uint8_t), SEEK_CUR);
	readBE(mFD, mFlags);
	// skip mode, mtimes, filename, checksum
	seek(3 * sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint32_t), SEEK_CUR);
	
	if (!readIndex()) {
		if (mib == Die)
			throw std::runtime_error("missing index");
		parseBlocks();
		if (mib == Write)
			writeIndex();
	}
	fprintf(stderr, "Ready\n");
}

// True on success
bool LzopFile::readIndex() {
	int fd = open(indexPath().c_str(), O_RDONLY);
	if (fd == -1)
		return false;
	
	uint32_t usize, csize;
	uint64_t uoff = 0, coff;
	while (true) {
		readBE(fd, usize);
		if (usize == 0)
			return true;
		readBE(fd, csize);
		readBE(fd, coff);
		
		mBlocks.push_back(Block(usize, csize, coff, uoff));
		uoff += usize;
	}
}

void LzopFile::writeIndex() const {
	int fd = open(indexPath().c_str(), O_WRONLY | O_CREAT, 0664);
	for (BlockList::const_iterator iter = mBlocks.begin();
			iter != mBlocks.end(); ++iter) {
		writeBE(fd, iter->usize);
		writeBE(fd, iter->csize);
		writeBE(fd, iter->coff);
	}
	uint32_t eof = 0;
	writeBE(fd, eof);
	close(fd);
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

void LzopFile::decompressBlock(const Block& b, Buffer& cbuf, Buffer& ubuf) {
	seek(b.coff, SEEK_SET);
	ubuf.resize(b.usize);
	
	if (b.csize == b.usize) { // Uncompressed, just read it
		read(mFD, &ubuf[0], b.usize);
		return;
	}
	
	cbuf.resize(b.csize);
	read(mFD, &cbuf[0], b.csize);
	
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
