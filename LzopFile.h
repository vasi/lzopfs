#ifndef LZOPFILE_H
#define LZOPFILE_H

#include "lzopfs.h"

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
	int mFD;
	uint32_t mFlags;
	BlockList mBlocks;
	
	void parseBlocks();
	std::string indexPath() const;
	bool readIndex();
	void writeIndex() const;
	
	off_t seek(off_t off, int whence);
	
	static void read(int fd, void *buf, size_t size);
	template <typename T> static void convertBE(T &t);
	template <typename T> static void readBE(int fd, T& t);
	template <typename T> static void writeBE(int fd, T t);

public:
	LzopFile(const std::string& path);
	
	BlockIterator findBlock(off_t off) const;
	void decompressBlock(const Block& b, Buffer& cbuf, Buffer& ubuf);
	
	off_t uncompressedSize() const;
};

#endif // LZOPFILE_H
