#ifndef BLOCKCACHE_H
#define BLOCKCACHE_H

#include "lzopfs.h"
#include "OpenCompressedFile.h"
#include "LRUMap.h"

class BlockCache {
protected:
	struct Key {
		OpenCompressedFile::FileID id;
		off_t offset;
		Key(OpenCompressedFile::FileID i, off_t o) : id(i), offset(o) { }
		bool operator==(const Key& o) const {
			return o.offset == offset && o.id == id;
		}
	};
	struct KeyHasher : public std::unary_function<Key, std::size_t> {
		size_t operator()(const Key& k) const {
			return std::tr1::hash<std::string>()(k.id) * 37 + k.offset;
		}
	};
	
	typedef LRUMap<Key, Buffer, KeyHasher> Map;
	Map mMap;

public:
	BlockCache(size_t maxSize = 0) : mMap(maxSize) { }
	void maxSize(size_t s) { mMap.maxWeight(s); }
	
	const Buffer& getBlock(OpenCompressedFile& file, const Block& block);
	
	void dump();
};

#endif // BLOCKCACHE_H
