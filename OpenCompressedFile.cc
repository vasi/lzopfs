#include "OpenCompressedFile.h"

#include "BlockCache.h"

OpenCompressedFile::OpenCompressedFile(LzopFile *lzop, int openFlags)
		: mLzop(lzop), mFH(lzop->path(), openFlags) { }

void OpenCompressedFile::decompressBlock(const Block& b, Buffer& ubuf) {
	mLzop->decompressBlock(mFH, b, ubuf);
}

void OpenCompressedFile::read(BlockCache& cache,
		char *buf, size_t size, off_t offset) {
	LzopFile::BlockIterator biter = mLzop->findBlock(offset);
	while (size > 0) {
		const Buffer &ubuf = cache.getBlock(*this, *biter);
		size_t bstart = offset - biter->uoff;
		size_t bsize = std::min(size, ubuf.size() - bstart);
		memcpy(buf, &ubuf[bstart], bsize);

		buf = reinterpret_cast<char*>(buf) + bsize;
		offset += bsize;
		size -= bsize;
		++biter;
	}
	
//	cache.dump();
}
