#ifndef OPENCOMPRESSEDFILE_H
#define OPENCOMPRESSEDFILE_H

#include "lzopfs.h"
#include "CompressedFile.h"
#include "FileHandle.h"

class BlockCache;

class OpenCompressedFile {
protected:
	const CompressedFile *mFile;
	FileHandle mFH;
	
public:
	typedef std::string FileID;
	
	OpenCompressedFile(const CompressedFile *file, int openFlags);
	
	void decompressBlock(const Block& b, Buffer& ubuf) const;
	ssize_t read(BlockCache& cache, char *buf, size_t size, off_t offset)
		const;
	
	FileID id() const { return mFile->path(); }
};

#endif // OPENCOMPRESSEDFILE_H
