#ifndef BZIP2FILE_H
#define BZIP2FILE_H

#include "lzopfs.h"
#include "CompressedFile.h"

#include <list>

class Bzip2File : public IndexedCompFile {
protected:
	virtual void checkFileType(FileHandle &fh);
	virtual void buildIndex(FileHandle& fh);
	
	struct BlockBoundary {
		uint64_t magic;
		char level;
		off_t coff;
		size_t bits;
		BlockBoundary(uint64_t m, char l, off_t c, size_t b)
			: magic(m), level(l), coff(c), bits(b) { }
	};
	typedef std::list<BlockBoundary> BoundList;
	
	struct Bzip2Block : public Block {
		size_t bits, endbits;
		char level;
		Bzip2Block() { }
		Bzip2Block(BlockBoundary& start, BlockBoundary& end, off_t uoff,
				size_t usize, char lev)
			: Block(usize, end.coff - start.coff, uoff, start.coff),
			bits(start.bits), endbits(end.bits), level(lev) { }
	};
	
	void findBlockBoundaryCandidates(FileHandle& fh, BoundList& bl) const;
	void createAlignedBlock(const FileHandle& fh, Buffer& b,
		char level, off_t coff, size_t bits, off_t end, size_t endbits) const;
	void decompress(const Buffer& in, Buffer& out) const;
	
	virtual Block* newBlock() const { return new Bzip2Block(); }
	virtual bool readBlock(FileHandle& fh, Block* b);
	virtual void writeBlock(FileHandle& fh, const Block *b) const;

public:
	static const char Magic[];
	static const uint64_t BlockMagic = 0x314159265359;
	static const size_t BlockMagicBytes = 6;
	static const uint64_t BlockMagicMask = (1LL << (BlockMagicBytes * 8)) - 1;
	static const uint64_t EOSMagic = 0x177245385090;
	
	static CompressedFile* open(const std::string& path, uint64_t maxBlock)
		{ return new Bzip2File(path, maxBlock); }
	
	Bzip2File(const std::string& path, uint64_t maxBlock)
		: IndexedCompFile(path) { initialize(maxBlock); }
	
	virtual std::string destName() const;
	
	virtual void decompressBlock(const FileHandle& fh, const Block& b,
		Buffer& ubuf) const;
};

#endif // BZIP2FILE_H
