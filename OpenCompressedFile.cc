#include "OpenCompressedFile.h"

#include "BlockCache.h"

#include <cstring>

OpenCompressedFile::OpenCompressedFile(const CompressedFile *file,
		int openFlags) : mFile(file), mFH(file->path(), openFlags) { }

void OpenCompressedFile::decompressBlock(const Block& b, Buffer& ubuf) const {
	mFile->decompressBlock(mFH, b, ubuf);
}

ssize_t OpenCompressedFile::read(BlockCache& cache,
		char *buf, size_t size, off_t offset) const {
	char *p = buf;
	CompressedFile::BlockIterator biter = mFile->findBlock(offset);
	for (; size > 0 && !biter.end(); ++biter) {
		BlockCache::BufPtr ubuf = cache.getBlock(*this, *biter);
		size_t bstart = offset - biter->uoff;
		size_t bsize = std::min(size, ubuf->size() - bstart);
		memcpy(p, &(*ubuf)[bstart], bsize);

		p += bsize;
		offset += bsize;
		size -= bsize;
	}
	
	// cache.dump();
	return p - buf;
}
