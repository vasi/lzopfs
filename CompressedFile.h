#ifndef COMPRESSEDFILE_H
#define COMPRESSEDFILE_H

#include "lzopfs.h"
#include "FileHandle.h"

#include <algorithm>
#include <string>

class CompressedFile {
public:
	class BlockIterator {
		// Disable copying
		BlockIterator &operator=(const BlockIterator& o);
		BlockIterator(const BlockIterator& o);
	
	protected:
		virtual void incr() = 0;
		
	public:
		BlockIterator() { }
		virtual ~BlockIterator() { }
		
		BlockIterator& operator++() { incr(); return *this; }
		const Block *operator->() const { return &**this; }
		
		virtual const Block& operator*() const = 0;
		virtual bool end() const = 0;
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
	virtual std::string suffix() const = 0;
	virtual std::string destName() const;
	
	virtual BlockIterator *findBlock(off_t off) const = 0;
	
	virtual void decompressBlock(FileHandle& fh, const Block& b,
		Buffer& ubuf) = 0;
	
	virtual off_t uncompressedSize() const = 0;
};

#endif // COMPRESSEDFILE_H
