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

namespace {
struct BlockOffsetOrdering {
	bool operator()(const Block& b, off_t off) {
		return (b.uoff + b.usize - 1) < (uint64_t)off;
	}
	
	bool operator()(off_t off, const Block& b) {
		return (uint64_t)off < b.uoff;
	}
};
}

CompressedFile::BlockIterator IndexedCompFile::findBlock(off_t off) const {
	BlockList::const_iterator iter = std::lower_bound(
		mBlocks.begin(), mBlocks.end(), off, BlockOffsetOrdering());
	if (iter == mBlocks.end())
		throw std::runtime_error("can't find block");
	return BlockIterator(new Iterator(iter, mBlocks.end()));
}

off_t IndexedCompFile::uncompressedSize() const {
	if (mBlocks.empty())
		return 0;
	const Block& b = mBlocks.back();
	return b.uoff + b.usize;
}

