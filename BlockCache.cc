#include "BlockCache.h"

void BlockCache::read(void *buf, size_t size, off_t off) {
	LzopFile::BlockIterator biter = mFile.findBlock(off);
	while (size > 0) {
		const Buffer &ubuf = cachedData(*biter);
		size_t bstart = off - biter->uoff;
		size_t bsize = std::min(size, ubuf.size() - bstart);
		memcpy(buf, &ubuf[bstart], bsize);
		
		off += bsize;
		size -= bsize;
		++biter;
	}
}

const Buffer& BlockCache::cachedData(const Block& block) {
	Buffer& buf = mMap[block.coff];
	if (buf.empty()) {
		// FIXME: Eject old cached blocks?
		mFile.decompressBlock(block, mCBuf, buf);
	}
	return buf;
}
