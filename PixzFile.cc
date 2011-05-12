#include "PixzFile.h"

#include "PathUtils.h"

#include <cassert>
#include <cstring>

#include <inttypes.h>

const uint64_t PixzFile::MemLimit = UINT64_MAX;

PixzFile::PixzFile(const std::string& path, uint64_t maxBlock)
		: CompressedFile(path), mIndex(0) {
	try {
		FileHandle fh(this->path(), O_RDONLY);
		Buffer header;
		fh.read(header, LZMA_STREAM_HEADER_SIZE);
		lzma_stream_flags flags;
		lzma_ret err = lzma_stream_header_decode(&flags, &header[0]);
		if (err == LZMA_FORMAT_ERROR)
			throwFormat("magic mismatch");
		else if (err == LZMA_DATA_ERROR)
			throwFormat("corrupt header");
		else if (err == LZMA_OPTIONS_ERROR)
			throwFormat("unsupported options in header");
		else if (err != LZMA_OK)
			throwFormat("bad header");
		
		mIndex = readIndex(fh);
	} catch (FileHandle::EOFException& e) {
		throwFormat("EOF");
	}
	
	checkSizes(maxBlock);
}
	
lzma_index *PixzFile::readIndex(FileHandle& fh) {
	assert(ChunkSize % 4 == 0);
	
	lzma_index *idx = 0;
	Buffer buf;
	lzma_stream s;
	streamInit(s);
	
	fh.seek(0, SEEK_END);
	while (true) {
		// Skip any padding.
		off_t pad = 0;
		while (true) {
			off_t p = fh.tell();
			if (p < LZMA_STREAM_HEADER_SIZE)
				throwFormat("padding not allowed at start");
			size_t s = std::min(p, off_t(ChunkSize));
			fh.seek(p - s, SEEK_SET);
			fh.read(buf, s);
			
			for (uint8_t *f = &buf[buf.size() - 4]; f >= &buf[0]; f -= 4) {
				if (*reinterpret_cast<uint32_t*>(f) != 0) {
					fh.seek(f - &buf[0] - buf.size() + 4, SEEK_CUR);
					goto footer;
				}
				pad += 4;
			}
			fh.seek(p - s, SEEK_SET);
		}
	
	footer:
		// Read the footer
		lzma_stream_flags flags;
		fh.seek(-LZMA_STREAM_HEADER_SIZE, SEEK_CUR);
		fh.read(buf, LZMA_STREAM_HEADER_SIZE);
		lzma_ret err = lzma_stream_footer_decode(&flags, &buf[0]);
		if (err != LZMA_OK)
			throwFormat("bad footer");
		off_t npos = fh.tell();
		
		// Read the index
		fh.seek(-LZMA_STREAM_HEADER_SIZE - flags.backward_size, SEEK_CUR);
		lzma_index *nidx;
		if (lzma_index_decoder(&s, &nidx, MemLimit) != LZMA_OK)
			throwFormat("error initializing index decoder");
		if (code(s, fh) != LZMA_OK)
			throwFormat("error decoding index");
		npos -= lzma_index_file_size(nidx);
		
		// Set index params, combine with any later indices
		if (lzma_index_stream_flags(nidx, &flags) != LZMA_OK)
			throwFormat("error setting stream flags");
		if (lzma_index_stream_padding(nidx, pad) != LZMA_OK)
			throwFormat("error setting stream padding");
		if (idx && lzma_index_cat(nidx, idx, NULL) != LZMA_OK)
			throwFormat("error combining indices");
		idx = nidx;
		
		if (npos == 0)
			return idx;
		fh.seek(npos, SEEK_SET);
	}
}

PixzFile::~PixzFile() {
	if (mIndex)
		lzma_index_end(mIndex, 0);
}

void PixzFile::streamInit(lzma_stream& s) const {
	// As suggested in docs for LZMA_STREAM_INIT
	memset(&s, 0, sizeof(lzma_stream));		
}

CompressedFile::BlockIterator PixzFile::findBlock(off_t off) const {
	lzma_index_iter *liter = new lzma_index_iter();
	lzma_index_iter_init(liter, mIndex);
	if (lzma_index_iter_locate(liter, off))
		throw std::runtime_error("can't find block");
	return BlockIterator(new Iterator(liter));
}

// Output should be already set up
lzma_ret PixzFile::code(lzma_stream& s, const FileHandle& fh, off_t off)
		const {
	try {
		if (off == -1)
			off = fh.tell();
	
		Buffer buf;
		lzma_ret err = LZMA_OK;
		s.avail_in = 0;
		while (err != LZMA_STREAM_END) {
			if (s.avail_in == 0) {
				s.avail_in = fh.tryPRead(off, buf, ChunkSize);
				off += buf.size();
				s.next_in = &buf[0];
			}
			err = lzma_code(&s, LZMA_RUN);
			if (err != LZMA_OK && err != LZMA_STREAM_END)
				break;
		}
		
		lzma_end(&s);
		if (err == LZMA_STREAM_END)
			err = LZMA_OK;
		return err;
		
	} catch (...) {
		lzma_end(&s);
		throw;
	}
}

void PixzFile::decompressBlock(const FileHandle& fh, const Block& b,
		Buffer& ubuf) const {	
//	fprintf(stderr, "Decompressing from %" PRIu64 "\n", uint64_t(b.coff));
	const PixzBlock& pb = dynamic_cast<const PixzBlock&>(b);
	
	// Read the block header
	lzma_block block;
	memset(&block, 0, sizeof(block));
	block.version = 0;
	block.check = pb.check;
	
	lzma_filter filters[LZMA_FILTERS_MAX + 1];
	filters[LZMA_FILTERS_MAX].id = LZMA_VLI_UNKNOWN;
	block.filters = filters;
	
	Buffer header;
	fh.pread(b.coff, header, 1);
	block.header_size = lzma_block_header_size_decode(header[0]);	
	header.resize(block.header_size);
	fh.pread(b.coff + 1, &header[1], block.header_size - 1);
	
	lzma_ret err = lzma_block_header_decode(&block, NULL, &header[0]);
	if (err == LZMA_DATA_ERROR)
		throwFormat("corrupt block header");
	else if (err == LZMA_OPTIONS_ERROR)
		throwFormat("unsupported options in block header");
	else if (err != LZMA_OK)
		throw std::runtime_error("unknown error in block header");
	
	
	// Decode the block
	lzma_stream s;
	streamInit(s);
	if (lzma_block_decoder(&s, &block) != LZMA_OK)
		throw std::runtime_error("error initializing block decoder");
	
	ubuf.resize(b.usize);
	s.next_out = &ubuf[0];
	s.avail_out = ubuf.size();
	if (code(s, fh, b.coff + block.header_size) != LZMA_OK)
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
	mBlock.check = mIter->stream.flags->check;		
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
