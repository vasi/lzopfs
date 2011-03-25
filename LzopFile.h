#ifndef LZOPFILE_H
#define LZOPFILE_H

#include "lzopfs.h"

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
	int mFD;
	uint32_t mFlags;
	BlockList mBlocks;
	
	void parseBlocks();
	
	off_t seek(off_t off, int whence);
	void seek(off_t off);
	void skip(off_t size);
	off_t tell();
	
	template <typename T> static void convertBE(T &t);
	template <typename T> void readBE(T& t);

	void read(void *buf, size_t size);

public:
	LzopFile(const char *path);
	
	BlockIterator findBlock(off_t off) const;
	void decompressBlock(const Block& b, Buffer& cbuf, Buffer& ubuf);
	
	off_t uncompressedSize() const;
};

#endif // LZOPFILE_H
