#include "PixzFile.h"

#include "PathUtils.h"

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

const uint64_t PixzFile::MemLimit = UINT64_MAX;

PixzFile::PixzFile(const std::string& path, uint64_t maxBlock)
		: CompressedFile(path), mIndex(0) {
	// As required in lzma/base.h
	memset(&mStream, 0, sizeof(lzma_stream));
	
	FileHandle fh(path, O_RDONLY);
	try {
		// Read the footer
		fh.seek(-LZMA_STREAM_HEADER_SIZE, SEEK_END);
		Buffer footer;
		fh.read(footer, LZMA_STREAM_HEADER_SIZE);
		lzma_ret err = lzma_stream_footer_decode(&mFlags, &footer[0]);
		if (err == LZMA_FORMAT_ERROR)
			throwFormat("magic mismatch");
		else if (err == LZMA_DATA_ERROR)
			throwFormat("corrupt footer");
		else if (err == LZMA_OPTIONS_ERROR)
			throwFormat("unsupported options in footer");
		else if (err != LZMA_OK)
			throwFormat("unknown error in footer");
		
		// Read the index
		fh.seek(-LZMA_STREAM_HEADER_SIZE - mFlags.backward_size);
		if (lzma_index_decoder(&mStream, &mIndex, MemLimit) != LZMA_OK)
			throwFormat("error initializing index decoder");
		if (code(fh) != LZMA_OK)
			throwFormat("error decoding index");
	} catch (FileHandle::EOFException& e) {
		throwFormat("EOF");
	}
	
	checkSizes(maxBlock);
}

PixzFile::~PixzFile() {
	lzma_index_end(mIndex, 0);
	lzma_end(&mStream);
}

CompressedFile::BlockIterator PixzFile::findBlock(off_t off) const {
	lzma_index_iter *liter = new lzma_index_iter();
	lzma_index_iter_init(liter, mIndex);
	if (lzma_index_iter_locate(liter, off))
		throw std::runtime_error("can't find block");
	return BlockIterator(new Iterator(liter));
}

// Output should be already set up
lzma_ret PixzFile::code(FileHandle& fh) {
	lzma_ret err = LZMA_OK;
	mStream.avail_in = 0;
	while (err != LZMA_STREAM_END) {
		if (mStream.avail_in == 0) {
			mStream.avail_in = fh.tryRead(mBuf, ChunkSize);
			mStream.next_in = &mBuf[0];
		}
		err = lzma_code(&mStream, LZMA_RUN);
		if (err != LZMA_OK && err != LZMA_STREAM_END)
			return err;
	}
	return LZMA_OK;
}

void PixzFile::decompressBlock(FileHandle& fh, const Block& b, Buffer& ubuf) {	
	fprintf(stderr, "Decompressing from %" PRIu64 "\n", uint64_t(b.coff));
	fh.seek(b.coff, SEEK_SET);
	
	
	// Read the block header
	lzma_block block;
	memset(&block, 0, sizeof(block));
	block.version = 0;
	block.check = mFlags.check;
	
	lzma_filter filters[LZMA_FILTERS_MAX + 1];
	filters[LZMA_FILTERS_MAX].id = LZMA_VLI_UNKNOWN;
	block.filters = filters;
	
	Buffer header;
	fh.read(header, 1);
	block.header_size = lzma_block_header_size_decode(header[0]);	
	header.resize(block.header_size);
	fh.read(&header[1], block.header_size - 1);
	
	lzma_ret err = lzma_block_header_decode(&block, NULL, &header[0]);
	if (err == LZMA_DATA_ERROR)
		throwFormat("corrupt block header");
	else if (err == LZMA_OPTIONS_ERROR)
		throwFormat("unsupported options in block header");
	else if (err != LZMA_OK)
		throw std::runtime_error("unknown error in block header");
	
	
	// Decode the block
	if (lzma_block_decoder(&mStream, &block) != LZMA_OK)
		throw std::runtime_error("error initializing block decoder");
	
	ubuf.resize(b.usize);
	mStream.next_out = &ubuf[0];
	mStream.avail_out = ubuf.size();
	if (code(fh) != LZMA_OK)
		throw std::runtime_error("error decoding block");
}

off_t PixzFile::uncompressedSize() const {
	return lzma_index_uncompressed_size(mIndex);
}

void PixzFile::Iterator::makeBlock() {
	mBlock.coff = mIter->block.compressed_file_offset;
	mBlock.uoff = mIter->block.uncompressed_file_offset;
	mBlock.csize = mIter->block.total_size;
	mBlock.usize = mIter->block.uncompressed_size;			
}

void PixzFile::Iterator::incr() {
	if (mEnd || lzma_index_iter_next(
			mIter, LZMA_INDEX_ITER_NONEMPTY_BLOCK)) {
		mEnd = true;
	} else {
		makeBlock();
	}
}

CompressedFile::BlockIteratorInner *PixzFile::Iterator::dup() const {
	if (mEnd)
		return new Iterator();
	
	// Safe according to lzma_index_iter_init docs
	return new Iterator(new lzma_index_iter(*mIter));
}

std::string PixzFile::destName() const {
	using namespace PathUtils;
	std::string base = basename(path());
	if (replaceExtension(base, "tpxz", "tar")) return base;
	if (replaceExtension(base, "txz", "tar")) return base;
	if (removeExtension(base, "pxz")) return base;
	if (removeExtension(base, "xz")) return base;
	return base;
}
