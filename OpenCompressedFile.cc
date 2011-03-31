#include "OpenCompressedFile.h"

#include "BlockCache.h"

#include <cstring>

OpenCompressedFile::OpenCompressedFile(LzopFile *lzop, int openFlags)
		: mLzop(lzop), mFH(lzop->path(), openFlags) { }

void OpenCompressedFile::decompressBlock(const Block& b, Buffer& ubuf) {
	mLzop->decompressBlock(mFH, b, ubuf);
}

ssize_t OpenCompressedFile::read(BlockCache& cache,
		char *buf, size_t size, off_t offset) {
	char *p = buf;
	CompressedFile::BlockIterator biter(mLzop->findBlock(offset));
	for (; size > 0 && !biter.end(); ++biter) {
		const Buffer &ubuf = cache.getBlock(*this, *biter);
		size_t bstart = offset - biter->uoff;
		size_t bsize = std::min(size, ubuf.size() - bstart);
		memcpy(p, &ubuf[bstart], bsize);

		p += bsize;
		offset += bsize;
		size -= bsize;
	}
	
//	cache.dump();
	return p - buf;
}
