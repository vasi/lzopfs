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

BlockCache::BufPtr BlockCache::getBlock(const OpenCompressedFile& file,
		const Block& block) {
	Key k(file.id(), block.coff);
	BufPtr *buf = mMap.find(k);
	if (buf)
		return *buf;
	
	// Add a new buffer
	BufPtr nbuf(new Buffer());
	file.decompressBlock(block, *nbuf);
	try {
		mMap.add(k, nbuf, block.usize);
	} catch (Map::OverWeight& e) {
		// that's ok!
	}
	return nbuf;
}
