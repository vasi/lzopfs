#ifndef BLOCKCACHE_H
#define BLOCKCACHE_H

#include <functional>
#include <memory>

#include "lzopfs.h"
#include "OpenCompressedFile.h"
#include "LRUMap.h"
#include "ThreadPool.h"

class BlockCache {
public:
	typedef std::shared_ptr<Buffer> BufPtr;

	struct Callback {
		virtual void operator()(const Block& block, BufPtr& buf) = 0;
	};

	typedef CompressedFile::BlockIterator BlockIterator;

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
			return std::hash<std::string>()(k.id) * 37 + k.offset;
		}
	};


	struct NeededBlock {
		BlockIterator biter;
		Key key;
		NeededBlock(const BlockIterator& bi, const Key& k)
			: biter(bi), key(k) { }
	};

	struct JobInfo {
		BlockCache& cache;
		const OpenCompressedFile& file;
		Callback& cb;
		ConditionVariable& cv;
		size_t& remain;
		JobInfo(BlockCache& c, const OpenCompressedFile& f, Callback& pcb,
			ConditionVariable& pcv, size_t& r)
			: cache(c), file(f), cb(pcb), cv(pcv), remain(r) { }
	};

	struct Job : public ThreadPool::Job {
		JobInfo& info;
		NeededBlock& block;

		Job(JobInfo& i, NeededBlock& b) : info(i), block(b) { }
		virtual void operator()();
	};
	friend struct Job;


	typedef LRUMap<Key, BufPtr, KeyHasher> Map;
	Map mMap;
	ThreadPool& mPool;
	Mutex mMutex;

public:
	BlockCache(ThreadPool& pool, size_t maxSize = 0)
		: mMap(maxSize), mPool(pool) { }
	void maxSize(size_t s) { mMap.maxWeight(s); }

	void dump();

	void getBlocks(const OpenCompressedFile& file, BlockIterator& it,
		off_t max, Callback& cb);
};

#endif // BLOCKCACHE_H
