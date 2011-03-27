#ifndef LZOPFILE_H
#define LZOPFILE_H

#include "lzopfs.h"
#include "FileHandle.h"

#include <string>

class LzopFile {
public:
	typedef std::vector<Block> BlockList;
	typedef BlockList::const_iterator BlockIterator;

	enum Flags {
		AdlerDec	= 1 << 0,
		AdlerComp	= 1 << 1,
		CRCDec		= 1 << 8,
		CRCComp		= 1 << 9,
	};
	
	static const char Magic[];

protected:
	std::string mPath;
	uint32_t mFlags;
	BlockList mBlocks;
	
	void parseHeader(FileHandle& fh, uint32_t& flags);
	void parseBlocks();
	std::string indexPath() const;
	bool readIndex();
	void writeIndex() const;
	
public:
	LzopFile(const std::string& path);
	
	BlockIterator findBlock(off_t off) const;
	void decompressBlock(FileHandle& fh, const Block& b, Buffer& ubuf);
	
	off_t uncompressedSize() const;
};

#endif // LZOPFILE_H
