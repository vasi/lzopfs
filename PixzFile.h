#ifndef PIXZFILE_H
#define PIXZFILE_H

#include "lzopfs.h"
#include "CompressedFile.h"

#include <lzma.h>

class PixzFile : public CompressedFile {
protected:	
	class Iterator : public BlockIteratorInner {
		lzma_index_iter *mIter;
		Block mBlock;
		bool mEnd;
		
		virtual void makeBlock();
		
		// Disable copy
		Iterator(const Iterator& o);
		Iterator& operator=(const Iterator& o);
	public:
		Iterator(lzma_index_iter *i) : mIter(i), mEnd(false) { makeBlock(); }
		Iterator() : mIter(0), mEnd(true) { }
		virtual ~Iterator() { if (mIter) delete mIter; }
		virtual void incr();
		virtual const Block& deref() const { return mBlock; }
		virtual bool end() const { return mEnd; }
		virtual BlockIteratorInner *dup() const;
	};
	
	
	static const uint64_t MemLimit;
	static const size_t ChunkSize;
	
	
	lzma_index *mIndex;
	lzma_stream_flags mFlags;
	
	lzma_stream mStream;
	Buffer mBuf;
	
	
	lzma_ret code(FileHandle& fh);
		
public:
	static CompressedFile* open(const std::string& path, uint64_t maxBlock)
		{ return new PixzFile(path, maxBlock); }
	
	PixzFile(const std::string& path, uint64_t maxBlock);
	virtual ~PixzFile();
		
	virtual std::string destName() const;
	
	virtual BlockIterator findBlock(off_t off) const;
	virtual void decompressBlock(FileHandle& fh, const Block& b,
		Buffer& ubuf);
	
	virtual off_t uncompressedSize() const;
};

#endif // PIXZFILE_H