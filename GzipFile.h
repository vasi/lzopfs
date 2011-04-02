#ifndef GZIPFILE_H
#define GZIPFILE_H

#include "lzopfs.h"
#include "CompressedFile.h"
#include "GzipReader.h"

class GzipFile : public IndexedCompFile {
protected:
	struct GzipBlock : public Block {
		size_t bits;
		Buffer dict;
		
		GzipBlock(off_t uoff, off_t coff, size_t b)
			: Block(0, 0, uoff, coff), bits(b) { }
	};
	
	
	void setLastBlockSize(off_t uoff, off_t coff);
	Buffer& addBlock(off_t uoff, off_t coff, size_t bits);
	
	virtual void checkFileType(FileHandle &fh);
	virtual bool readIndex(FileHandle& fh);
	virtual void buildIndex(FileHandle& fh);
	virtual void writeIndex(FileHandle& fh) const;
	
public:
	static const size_t WindowSize;
	static const uint64_t MinDictBlockSize;
	
	static CompressedFile* open(const std::string& path, uint64_t maxBlock)
		{ return new GzipFile(path, maxBlock); }
	
	GzipFile(const std::string& path, uint64_t maxBlock);
	
	virtual std::string destName() const;
	
	virtual void decompressBlock(FileHandle& fh, const Block& b,
		Buffer& ubuf);
};

#endif // GZIPFILE_H
