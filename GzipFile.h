#ifndef GZIPFILE_H
#define GZIPFILE_H

#include "lzopfs.h"
#include "CompressedFile.h"

#include <zlib.h>

class GzipFile : public IndexedCompFile {
protected:
	static const size_t WindowSize;
	
	z_stream mStream;
	Buffer mInBuf;
	
	void setupStream(z_stream& s);
	void ensureInput(z_stream& s, FileHandle& fh);
	bool ensureOutput(z_stream& s, Buffer& w, bool force = false);
	
	
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
	virtual ~GzipFile() { inflateEnd(&mStream); }
	
	virtual std::string destName() const;
	
	virtual BlockIterator findBlock(off_t off) const;
	virtual void decompressBlock(FileHandle& fh, const Block& b,
		Buffer& ubuf);
	
	virtual off_t uncompressedSize() const;
};

#endif // GZIPFILE_H
