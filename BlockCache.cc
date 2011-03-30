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
	
	fprintf(stderr, "\nCache:\n");
	for (LRUMap<off_t, Buffer>::Iterator iter = mMap.begin();
			iter != mMap.end(); ++iter) {
		fprintf(stderr, "  %lld\n", iter->key);
	}
}

const Buffer& BlockCache::cachedData(FileHandle& fh, const Block& block) {
	off_t coff = block.coff;
	Buffer *found = mMap.find(coff);
	if (found)
		return *found;
	
	// Add a new buffer
	Buffer &buf = mMap.add(coff, Buffer(), block.usize).value;
	mFile->decompressBlock(fh, block, buf);
	return buf;
}
