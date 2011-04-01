#include "CompressedFile.h"

#include "PathUtils.h"

void CompressedFile::throwFormat(const std::string& s) const {
	throw FormatException(mPath, s);
}

std::string CompressedFile::destName() const {
	return PathUtils::removeExtension(PathUtils::basename(path()),
		this->suffix());
}

void CompressedFile::checkSizes(uint64_t maxBlock) const {
	for (BlockIterator iter = this->findBlock(0); !iter.end(); ++iter) {
		if (iter->usize > maxBlock) {
			fprintf(stderr, "WARNING: %s has blocks too large to cache, "
				"operations on it will be slow!\n", path().c_str());
			break;
		}
	}
}
