#include "BlockCache.h"

#include <cstdio>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

void BlockCache::dump() {
	Map::Iterator iter;
	size_t blocks = 0;
	
	for (iter = mMap.begin(); iter != mMap.end(); ++iter)
		++blocks;
	fprintf(stderr, "\nCache: %3zu blocks, %5.2f MB\n",
		blocks, mMap.weight() / 1024.0 / 1024);
	
	for (iter = mMap.begin(); iter != mMap.end(); ++iter) {
		fprintf(stderr, "  %9" PRIu64 " %s\n", uint64_t(iter->key.offset),
			iter->key.id.c_str());
	}
}

BlockCache::CachedBuffer BlockCache::getBlock(OpenCompressedFile& file,
		const Block& block) {
	Key k(file.id(), block.coff);
	Buffer *buf = mMap.find(k);
	bool owned = false;
	
	if (!buf) {
		// Add a new buffer
		try {
			buf = &mMap.add(k, Buffer(), block.usize).value;
		} catch (Map::OverWeight& e) {
			buf = new Buffer();
			owned = true; 
		}
		file.decompressBlock(block, *buf);
	}
	return CachedBuffer(buf, owned);
}
