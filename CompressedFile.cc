#include "CompressedFile.h"

#include "PathUtils.h"

void CompressedFile::throwFormat(const std::string& s) const {
	throw FormatException(mPath, s);
}

std::string CompressedFile::destName() const {
	return PathUtils::basename(path());
}

void CompressedFile::checkSizes(uint64_t maxBlock) const {
	for (BlockIterator iter = findBlock(0); !iter.end(); ++iter) {
		if (iter->usize > maxBlock) {
			fprintf(stderr, "WARNING: %s has blocks too large to cache, "
				"operations on it will be slow!\n", path().c_str());
			break;
		}
	}
}

void IndexedCompFile::initialize(uint64_t maxBlock) {
	FileHandle fh(path(), O_RDONLY);
	checkFileType(fh);
	
	// Try reading the index
	bool index = false;
	{
		FileHandle idxr;
		try {
			idxr.open(indexPath(), O_RDONLY);
		} catch (FileHandle::Exception& e) {
			// ok to fail
		}
		if (idxr.open() && readIndex(idxr))
			index = true;
	}
	
	if (!index) {
		buildIndex(fh);
		FileHandle idxw(indexPath(), O_WRONLY | O_CREAT, 0664);
		writeIndex(idxw);
	}

	checkSizes(maxBlock);
}

std::string IndexedCompFile::indexPath() const {
	return path() + ".blockIdx";
}