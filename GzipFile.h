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
	virtual void buildIndex(FileHandle& fh);
	
	virtual Block* newBlock() const { return new GzipBlock(0, 0, 0); }
	virtual bool readBlock(FileHandle& fh, Block* b);	// True unless EOF
	virtual void writeBlock(FileHandle& fh, const Block *b) const;

public:
	static const size_t WindowSize;
	static uint64_t gMinDictBlockFactor;
	
	static CompressedFile* open(const std::string& path, uint64_t maxBlock)
		{ return new GzipFile(path, maxBlock); }
	
	GzipFile(const std::string& path, uint64_t maxBlock);
	
	virtual std::string destName() const;
	
	virtual void decompressBlock(const FileHandle& fh, const Block& b,
		Buffer& ubuf) const;
};

#endif // GZIPFILE_H
