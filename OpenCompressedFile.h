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
	typedef std::string FileID;
	
	OpenCompressedFile(LzopFile *lzop, int openFlags);
	
	void decompressBlock(const Block& b, Buffer& ubuf);
	ssize_t read(BlockCache& cache, char *buf, size_t size, off_t offset);
	
	FileID id() const { return mLzop->path(); }
};

#endif // OPENCOMPRESSEDFILE_H
