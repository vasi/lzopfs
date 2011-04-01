#ifndef COMPRESSEDFILE_H
#define COMPRESSEDFILE_H

#include "lzopfs.h"
#include "FileHandle.h"

#include <algorithm>
#include <string>

class CompressedFile {
public:
	class BlockIteratorInner {
	public:
		virtual void incr() = 0;
		virtual bool end() const = 0;
		virtual const Block& deref() const = 0;
		virtual BlockIteratorInner *dup() const = 0;
	};
	
	class BlockIterator {
		BlockIteratorInner *mInner;
	
	public:
		BlockIterator(BlockIteratorInner *i) : mInner(i) { }
		virtual ~BlockIterator() { delete mInner; }
		
		BlockIterator& operator++() { mInner->incr(); return *this; }
		const Block *operator->() const { return &**this; }
		const Block& operator*() const { return mInner->deref(); }
		bool end() const { return mInner->end(); }
				
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
	
	virtual void decompressBlock(FileHandle& fh, const Block& b,
		Buffer& ubuf) = 0;
	
	virtual off_t uncompressedSize() const = 0;
};

class IndexedCompFile : public CompressedFile {
public:
	IndexedCompFile(const std::string& path)
		: CompressedFile(path) { }

protected:	
	virtual std::string indexPath() const;
	virtual void initialize(uint64_t maxBlock);
	
	virtual void checkFileType(FileHandle &fh) = 0;
	virtual bool readIndex(FileHandle& fh) = 0;
	virtual void buildIndex(FileHandle& fh) = 0;
	virtual void writeIndex(FileHandle& fh) const = 0;
};

#endif // COMPRESSEDFILE_H
