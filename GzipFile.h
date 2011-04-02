#ifndef GZIPFILE_H
#define GZIPFILE_H

#include "lzopfs.h"
#include "CompressedFile.h"
#include "GzipReader.h"

class GzipFile : public IndexedCompFile {
protected:
	class Iterator : public BlockIteratorInner {
		// FIXME
		Block mBlock;
		virtual void incr() { }
		virtual const Block& deref() const { return mBlock; }
		virtual bool end() const { return true; }
		virtual BlockIteratorInner *dup() const
			{ return new Iterator(); }
	};
	
	virtual void checkFileType(FileHandle &fh);
	virtual bool readIndex(FileHandle& fh);
	virtual void buildIndex(FileHandle& fh);
	virtual void writeIndex(FileHandle& fh) const;
	
public:
	static const size_t WindowSize;	
	static const uint64_t MaxDictBlockSize;
	
	static CompressedFile* open(const std::string& path, uint64_t maxBlock)
		{ return new GzipFile(path, maxBlock); }
	
	GzipFile(const std::string& path, uint64_t maxBlock);
	
	virtual std::string destName() const;
	
	virtual BlockIterator findBlock(off_t off) const;
	virtual void decompressBlock(FileHandle& fh, const Block& b,
		Buffer& ubuf);
	
	virtual off_t uncompressedSize() const;
};

#endif // GZIPFILE_H
