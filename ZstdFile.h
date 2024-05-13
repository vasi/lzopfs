#ifndef ZSTDFILE_H
#define ZSTDFILE_H

#include "lzopfs.h"
#include "CompressedFile.h"
#include "FileList.h"

#include <zstd.h>

class ZstdFile : public BlockListCompFile {
protected:
	struct SeekTableInfo {
		uint64_t start;
		bool hasChecksums;
		uint32_t count;

		size_t entrySize() const {
			return 8 + (hasChecksums ? 4 : 0);
		}

		size_t totalSize() const {
			return entrySize() * count;
		}
	};

	SeekTableInfo findSeekTable(FileHandle& fh) const;

public:
	static CompressedFile* open(const std::string& path, const OpenParams& params)
		{ return new ZstdFile(path, params.maxBlock); }
	
	ZstdFile(const std::string& path, uint64_t maxBlock);
	
	virtual std::string destName() const;
	virtual void checkFileType(FileHandle &fh);

	virtual void decompressBlock(const FileHandle& fh, const Block& b,
		Buffer& ubuf) const;

	virtual void buildIndex(FileHandle& fh);
};

#endif // ZSTDFILE_H
