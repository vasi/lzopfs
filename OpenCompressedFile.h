#ifndef OPENCOMPRESSEDFILE_H
#define OPENCOMPRESSEDFILE_H

#include "lzopfs.h"
#include "LzopFile.h"
#include "FileHandle.h"

class BlockCache;

class OpenCompressedFile {
	LzopFile *mLzop;
	FileHandle mFH;
	
public:
	OpenCompressedFile(LzopFile *lzop, int openFlags);
	
	void decompressBlock(const Block& b, Buffer& ubuf);
	void read(BlockCache& cache, char *buf, size_t size, off_t offset);
};

#endif // OPENCOMPRESSEDFILE_H
