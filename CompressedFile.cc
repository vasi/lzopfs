#include "CompressedFile.h"

#include "PathUtils.h"

void CompressedFile::throwFormat(const std::string& s) const {
	throw FormatException(mPath, s);
}

std::string CompressedFile::destName() const {
	return PathUtils::removeExtension(PathUtils::basename(path()),
		this->suffix());
}
