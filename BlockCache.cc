#include "BlockCache.h"

void BlockCache::dump() {
	LRUMap<off_t, Buffer>::Iterator iter;
	size_t blocks = 0;
	
	for (iter = mMap.begin(); iter != mMap.end(); ++iter)
		++blocks;
	fprintf(stderr, "\nCache: %3zu blocks, %5.2f MB\n",
		blocks, mMap.weight() / 1024.0 / 1024);
	
	for (iter = mMap.begin(); iter != mMap.end(); ++iter) {
		fprintf(stderr, "  %lld\n", iter->key);
	}
}

const Buffer& BlockCache::getBlock(OpenCompressedFile& file,
		const Block& block) {
	Buffer *found = mMap.find(block.coff);
	if (found)
		return *found;
	
	// Add a new buffer
	Buffer &buf = mMap.add(block.coff, Buffer(), block.usize).value;
	file.decompressBlock(block, buf);
	return buf;
}
