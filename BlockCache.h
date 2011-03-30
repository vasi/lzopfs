#ifndef BLOCKCACHE_H
#define BLOCKCACHE_H

#include "lzopfs.h"
#include "OpenCompressedFile.h"
#include "LRUMap.h"

class BlockCache {
protected:
	LRUMap<off_t, Buffer> mMap;

public:
	BlockCache(size_t maxSize = 0) : mMap(maxSize) { }
	void maxSize(size_t s) { mMap.maxWeight(s); }
	
	const Buffer& getBlock(OpenCompressedFile& file, const Block& block);
	
	void dump();
};

#endif // BLOCKCACHE_H
