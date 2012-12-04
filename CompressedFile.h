#ifndef COMPRESSEDFILE_H
#define COMPRESSEDFILE_H

#include "lzopfs.h"
#include "FileHandle.h"

#include <algorithm>
#include <string>

class CompressedFile {
public:
	static const size_t ChunkSize; // for input buffers
	
	
	class BlockIteratorInner {
	public:
		virtual void incr() = 0;
		virtual bool end() const = 0;
		virtual const Block& deref() const = 0;
		virtual BlockIteratorInner *dup() const = 0;
		virtual ~BlockIteratorInner() { }
	};
	
	class BlockIterator {
		BlockIteratorInner *mInner;
	
	public:
		BlockIterator(BlockIteratorInner *i) : mInner(i) { }
		virtual ~BlockIterator() { if (mInner) delete mInner; }
		
		BlockIterator& operator++() { mInner->incr(); return *this; }
		const Block *operator->() const { return &**this; }
		const Block& operator*() const { return mInner->deref(); }
		bool end() const { return mInner->end(); }
		
		BlockIterator() : mInner(0) { }
		BlockIterator(const BlockIterator& o) : mInner(o.mInner->dup()) { }
		BlockIterator &operator=(BlockIterator o)
			{ std::swap(mInner, o.mInner); return *this; }
	};
	
	
	struct FormatException : public virtual std::runtime_error {
		std::string file;
		FormatException(const std::string& f, const std::string& s)
			: std::runtime_error(s), file(f) { }
		~FormatException() throw() { }
	};

protected:
	std::string mPath;
	
	virtual void throwFormat(const std::string& s) const;
	virtual void checkSizes(uint64_t maxBlock) const;

public:
	CompressedFile(const std::string& path) : mPath(path) { }
	virtual ~CompressedFile() { }
	
	virtual const std::string& path() const { return mPath; }
	virtual std::string destName() const;
	
	virtual BlockIterator findBlock(off_t off) const = 0;
	
	virtual void decompressBlock(const FileHandle& fh, const Block& b,
		Buffer& ubuf) const = 0;
	
	virtual off_t uncompressedSize() const = 0;
	
	void dumpBlocks();
};

class IndexedCompFile : public CompressedFile {
public:
	IndexedCompFile(const std::string& path)
		: CompressedFile(path) { }
	virtual ~IndexedCompFile();

protected:	
	typedef std::vector<Block*> BlockList;
	BlockList mBlocks;	
	
	
	class Iterator : public BlockIteratorInner {
		BlockList::const_iterator mIter, mEnd;
	public:
		Iterator(BlockList::const_iterator i, BlockList::const_iterator e)
			: mIter(i), mEnd(e) { }
		virtual void incr() { ++mIter; }
		virtual const Block& deref() const { return **mIter; }
		virtual bool end() const { return mIter == mEnd; }
		virtual BlockIteratorInner *dup() const
			{ return new Iterator(mIter, mEnd); }
	};
	
	
	virtual std::string indexPath() const;
	virtual void initialize(uint64_t maxBlock);
	
	virtual void checkFileType(FileHandle &fh) = 0;
	virtual void buildIndex(FileHandle& fh) = 0;
	
	virtual bool readIndex(FileHandle& fh); // True on success
	virtual void writeIndex(FileHandle& fh) const;
	virtual Block* newBlock() const { return new Block(); }
	virtual bool readBlock(FileHandle& fh, Block* b);	// True unless EOF
	virtual void writeBlock(FileHandle& fh, const Block *b) const;
	
	void addBlock(Block* b) { mBlocks.push_back(b); }
	virtual BlockIterator findBlock(off_t off) const;
	virtual off_t uncompressedSize() const;
};

#endif // COMPRESSEDFILE_H
