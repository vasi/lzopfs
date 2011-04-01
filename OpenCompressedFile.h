#ifndef OPENCOMPRESSEDFILE_H
#define OPENCOMPRESSEDFILE_H

#include "lzopfs.h"
#include "CompressedFile.h"
#include "FileHandle.h"

class BlockCache;

class OpenCompressedFile {
protected:
	CompressedFile *mFile;
	FileHandle mFH;
	
public:
	typedef std::string FileID;
	
	OpenCompressedFile(CompressedFile *file, int openFlags);
	
	void decompressBlock(const Block& b, Buffer& ubuf);
	ssize_t read(BlockCache& cache, char *buf, size_t size, off_t offset);
	
	FileID id() const { return mFile->path(); }
};

#endif // OPENCOMPRESSEDFILE_H
