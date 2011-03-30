#ifndef BLOCKCACHE_H
#define BLOCKCACHE_H

#include "lzopfs.h"
#include "LzopFile.h"
#include "LRUMap.h"

class BlockCache {
protected:
	LzopFile *mFile;
	LRUMap<off_t, Buffer> mMap;
	const Buffer& cachedData(FileHandle& fh, const Block& block);

public:
	const static size_t DefaultMaxSize;
	
	BlockCache(LzopFile *file, size_t maxsz = DefaultMaxSize)
		: mFile(file), mMap(maxsz) { }
	
	void read(int fd, void *buf, size_t size, off_t off);
	
	const LzopFile* file() const { return mFile; }
};

#endif // BLOCKCACHE_H
