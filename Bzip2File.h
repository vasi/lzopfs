#ifndef BZIP2FILE_H
#define BZIP2FILE_H

#include "lzopfs.h"
#include "CompressedFile.h"

class Bzip2File : public IndexedCompFile {
protected:
	virtual void checkFileType(FileHandle &fh);
	virtual void buildIndex(FileHandle& fh);
	
	struct Bzip2Block : public Block {
		size_t bits;
		Bzip2Block(off_t coff, size_t b) : Block(0, 0, 0, coff), bits(b) { }
	};
	
	void findBlockBoundaryCandidates(FileHandle& fh);
public:
	static const char Magic[];
	static const uint64_t BlockMagic = 0x314159265359;
	static const size_t BlockMagicBytes = 6;
	static const uint64_t BlockMagicMask = (1LL << (BlockMagicBytes * 8)) - 1;
	
	static CompressedFile* open(const std::string& path, uint64_t maxBlock)
		{ return new Bzip2File(path, maxBlock); }
	
	Bzip2File(const std::string& path, uint64_t maxBlock)
		: IndexedCompFile(path) { initialize(maxBlock); }
	
	virtual std::string destName() const;
	
	virtual void decompressBlock(FileHandle& fh, const Block& b,
		Buffer& ubuf);
};

#endif // BZIP2FILE_H
