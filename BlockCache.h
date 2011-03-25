#ifndef BLOCKCACHE_H
#define BLOCKCACHE_H

#include "lzopfs.h"
#include "LzopFile.h"

#include <list>
#include <tr1/unordered_map>

class BlockCache {
protected:
	LzopFile mFile;
	size_t mMaxSize, mSize;
	
	// Least-recently-used cache
	struct AgeEntry {
		Buffer buf;
		off_t coff;
		AgeEntry(off_t o) : coff(o) { }
	};
	typedef std::list<AgeEntry> AgeList;
	typedef std::tr1::unordered_map<off_t,AgeList::iterator> Map;
	
	AgeList mOld; // new buffers at the head, old at the tail
	Map mMap;
	Buffer mCBuf;
	
	const Buffer& cachedData(const Block& block);

public:
	const static size_t DefaultMaxSize;
	
	BlockCache(const char *path, size_t maxsz = DefaultMaxSize)
		: mFile(path), mMaxSize(maxsz) { }
	
	void read(void *buf, size_t size, off_t off);
	
	const LzopFile& file() const { return mFile; }
};

#endif // BLOCKCACHE_H
