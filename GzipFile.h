#ifndef GZIPFILE_H
#define GZIPFILE_H

#include "lzopfs.h"
#include "CompressedFile.h"

#include <zlib.h>

class GzipFile : public IndexedCompFile {
protected:
	static const size_t Chunk;
	
	z_stream mStream;
	
	void setupStream(z_stream& s);
	
	
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
