#include "CompressedFile.h"

#include <cstdio>

#include "PathUtils.h"

#include <inttypes.h>

const size_t CompressedFile::ChunkSize = 4096;

void CompressedFile::throwFormat(const std::string& s) const {
	throw FormatException(mPath, s);
}

std::string CompressedFile::destName() const {
	return PathUtils::basename(path());
}

void CompressedFile::checkSizes(uint64_t maxBlock) const {
	BlockIterator iter;
	try {
		iter = findBlock(0);
	} catch (std::runtime_error& e) {
		throw std::runtime_error("no blocks in file?");
	}
	for (; !iter.end(); ++iter) {
		if (iter->usize > maxBlock) {
			fprintf(stderr, "WARNING: %s has blocks too large to cache, "
				"operations on it will be slow!\n", path().c_str());
			break;
		}
	}
}

void CompressedFile::dumpBlocks() {
	fprintf(stderr, "\nBLOCKS\n");
	for (BlockIterator iter = findBlock(0); !iter.end(); ++iter) {
		fprintf(stderr, "Block: uoff = %9" PRIu64 ", coff = %9" PRIu64
			", usize = %9u, csize = %9u\n", iter->uoff, iter->coff,
			iter->usize, iter->csize);
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
		FileHandle idxw(indexPath(), O_WRONLY | O_CREAT | O_TRUNC, 0664);
		writeIndex(idxw);
	}

	checkSizes(maxBlock);
}

std::string IndexedCompFile::indexPath() const {
	return path() + ".blockIdx";
}

namespace {
struct BlockOffsetOrdering {
	bool operator()(const Block* b, off_t off) {
		return (b->uoff + b->usize - 1) < (uint64_t)off;
	}
	
	bool operator()(off_t off, const Block* b) {
		return (uint64_t)off < b->uoff;
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

IndexedCompFile::~IndexedCompFile() {
	BlockList::iterator iter;
	for (iter = mBlocks.begin(); iter != mBlocks.end(); ++iter)
		delete *iter;
}

off_t IndexedCompFile::uncompressedSize() const {
	if (mBlocks.empty())
		return 0;
	const Block& b = *mBlocks.back();
	return b.uoff + b.usize;
}

bool IndexedCompFile::readIndex(FileHandle& fh) {
	uint64_t uoff = 0;
	while (true) {
		Block* b = 0;
		try {
			b = newBlock();
			if (!readBlock(fh, b)) {
				delete b;
//				dumpBlocks();
				return true;
			}
			b->uoff = uoff;
			addBlock(b);
			uoff += b->usize;
		} catch (...) {
			delete b;
			throw;
		}
	}
}

bool IndexedCompFile::readBlock(FileHandle& fh, Block *b) {
	fh.readBE(b->usize);
	if (b->usize == 0)
		return false;
	fh.readBE(b->csize);
	fh.readBE(b->coff);
	// uoff will be calculated
	return true;
}

void IndexedCompFile::writeIndex(FileHandle& fh) const {
	for (BlockList::const_iterator iter = mBlocks.begin();
			iter != mBlocks.end(); ++iter) {
		writeBlock(fh, *iter);
	}
	uint32_t eof = 0;
	fh.writeBE(eof);
//	fprintf(stderr, "Wrote index\n");
}

void IndexedCompFile::writeBlock(FileHandle& fh, const Block* b) const {
	fh.writeBE(b->usize);
	fh.writeBE(b->csize);
	fh.writeBE(b->coff);	
}
