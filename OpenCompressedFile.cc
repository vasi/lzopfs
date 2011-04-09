#include "OpenCompressedFile.h"

#include "BlockCache.h"

#include <cstring>

OpenCompressedFile::OpenCompressedFile(const CompressedFile *file,
		int openFlags) : mFile(file), mFH(file->path(), openFlags) { }

void OpenCompressedFile::decompressBlock(const Block& b, Buffer& ubuf) const {
	mFile->decompressBlock(mFH, b, ubuf);
}

namespace {
	struct Callback : public BlockCache::Callback {
		off_t& max;
		char *buf;
		size_t size;
		off_t offset;
		
		Callback(off_t& m, char *b, size_t s, off_t o)
			: max(m), buf(b), size(s), offset(o) { }
		
		virtual void operator()(const Block& block,
				BlockCache::BufPtr& ubuf) {
			off_t omin = std::max(offset, off_t(block.uoff)),
				omax = std::min(offset + size, block.uoff + block.usize);
			size_t bstart = omin - block.uoff,
				bsize = omax - omin;
			memcpy(buf + omin - offset, &(*ubuf)[bstart], bsize);
			max = omax;
		}
	};
}

ssize_t OpenCompressedFile::read(BlockCache& cache,
		char *buf, size_t size, off_t offset) const {
	off_t max = offset;
	CompressedFile::BlockIterator biter = mFile->findBlock(offset);
	Callback cb(max, buf, size, offset);
	cache.getBlocks(*this, biter, offset + size, cb);
	
	return max - offset;
}
