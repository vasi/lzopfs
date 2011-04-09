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

#if 0
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
#endif

void BlockCache::Job::operator()() {
	bool done = false;
	{
		// We could have acquired the block between queuing and runnnig
		Lock lock(info.cache.mMutex);
		BufPtr *buf = info.cache.mMap.find(block.key);
		if (buf) {
			info.cb(*block.biter, *buf);
			done = true;
		}
	}
	
	if (!done) {
		BufPtr nbuf(new Buffer());
		info.file.decompressBlock(*block.biter, *nbuf);
		Lock lock(info.cache.mMutex);
		try {
			info.cache.mMap.add(block.key, nbuf, nbuf->size());
		} catch (Map::OverWeight& e) {
			// that's ok!
		}
		info.cb(*block.biter, nbuf);
	}
	
	Lock lock(info.cv);
	if (--info.remain == 0)
		info.cv.signal();
}

void BlockCache::getBlocks(const OpenCompressedFile& file, BlockIterator& it,
		off_t max, Callback& cb) {
	std::vector<NeededBlock> need;	
	{
		Lock lock(mMutex);
		for (; !it.end() && it->uoff < max; ++it) {
			Key k(file.id(), it->coff);
			BufPtr *buf = mMap.find(k);
			if (buf)
				cb(*it, *buf);
			else
				need.push_back(NeededBlock(it, k));
		}
	}
	if (need.empty())
		return;
	
	ConditionVariable cv;
	Lock clock(cv);
	size_t remain = need.size();
	JobInfo info(*this, file, cb, cv, remain);
	for (std::vector<NeededBlock>::iterator nb = need.begin();
			nb != need.end(); ++nb) {
		mPool.enqueue(new Job(info, *nb));
	}
	cv.wait();
}
