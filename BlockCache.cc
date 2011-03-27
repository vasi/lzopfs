#include "BlockCache.h"

const size_t BlockCache::DefaultMaxSize = 1024 * 1024 * 32;

void BlockCache::read(int fd, void *buf, size_t size, off_t off) {
	FileHandle fh(fd);
	LzopFile::BlockIterator biter = mFile->findBlock(off);
	while (size > 0) {
		const Buffer &ubuf = cachedData(fh, *biter);
		size_t bstart = off - biter->uoff;
		size_t bsize = std::min(size, ubuf.size() - bstart);
		memcpy(buf, &ubuf[bstart], bsize);
		
		buf = reinterpret_cast<char*>(buf) + bsize;
		off += bsize;
		size -= bsize;
		++biter;
	}
}

const Buffer& BlockCache::cachedData(FileHandle& fh, const Block& block) {
	off_t coff = block.coff;
	Map::iterator miter = mMap.find(coff);
	if (miter == mMap.end()) {
		// Add a new buffer and fill it
		mOld.push_front(AgeEntry(coff));
		mMap[coff] = mOld.begin();
		Buffer &buf = mOld.front().buf;
		mFile->decompressBlock(fh, block, buf);
		mSize += buf.size();
		
		// Remove old ones from the end
		AgeList::iterator aiter = --mOld.end();
		while (mSize > mMaxSize) {
			fprintf(stderr, "Removing old buffer %lld\n", aiter->coff);
			mSize -= aiter->buf.size();
			mMap.erase(aiter->coff);
			mOld.erase(aiter);
			--aiter;
		}
		return buf;
	} else {
		// Mark it as new
		mOld.splice(mOld.begin(), mOld, miter->second);
		return miter->second->buf;
	}
}
