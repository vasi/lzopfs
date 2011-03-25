#ifndef BLOCKCACHE_H
#define BLOCKCACHE_H

#include "lzopfs.h"
#include "LzopFile.h"

#include <tr1/unordered_map>

class BlockCache {
protected:
	typedef std::tr1::unordered_map<off_t,Buffer> Map;

	LzopFile mFile;
	Map mMap;
	Buffer mCBuf;
	
	const Buffer& cachedData(const Block& block);

public:	
	BlockCache(const char *path) : mFile(path) { }
	
	void read(void *buf, size_t size, off_t off);
	
	const LzopFile& file() const { return mFile; }
};

#endif // BLOCKCACHE_H
