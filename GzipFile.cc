#include "GzipFile.h"

#include "PathUtils.h"

const size_t GzipFile::Chunk = 4096;

void GzipFile::setupStream(z_stream& s) {
	s.zalloc = Z_NULL;
	s.zfree = Z_NULL;
	s.opaque = Z_NULL;
}

void GzipFile::checkFileType(FileHandle& fh) {
	try {
		// Read til end of header
		Buffer buf;
		if (inflateInit2(&mStream, 16 + MAX_WBITS) != Z_OK)
			throwFormat("can't init gzip reader");
		
		gz_header header;
		if (inflateGetHeader(&mStream, &header) != Z_OK)
			throwFormat("can't init gzip header");
		
		// inflate wants a next_out, even if it won't use it
		unsigned char dummy;
		mStream.next_out = &dummy;
		mStream.avail_out = 0;
		
		mStream.avail_in = 0;
		while (!header.done) {
			if (mStream.avail_in == 0) {
				mStream.avail_in = fh.tryRead(buf, Chunk);
				mStream.next_in = &buf[0];
			}
			if (inflate(&mStream, Z_BLOCK) != Z_OK)
				throwFormat("can't read gzip header");
		}
		
		fprintf(stderr, "got here!\n"); exit(1);
	} catch (FileHandle::EOFException& e) {
		throwFormat("EOF");
	}
}

void GzipFile::buildIndex(FileHandle& fh) {
	// FIXME
}

GzipFile::GzipFile(const std::string& path, uint64_t maxBlock)
		: IndexedCompFile(path) {
	setupStream(mStream);
	initialize(maxBlock);
}

bool GzipFile::readIndex(FileHandle& fh) {
	return false; // FIXME
}

void GzipFile::writeIndex(FileHandle& fh) const {
	// FIXME
}

CompressedFile::BlockIterator GzipFile::findBlock(off_t off) const {
	return BlockIterator(new Iterator()); // FIXME
}

void GzipFile::decompressBlock(FileHandle& fh, const Block& b,
		Buffer& ubuf) {
	// FIXME
}

off_t GzipFile::uncompressedSize() const {
	return 0; // FIXME
}

std::string GzipFile::destName() const {
	using namespace PathUtils;
	std::string base = basename(path());
	if (replaceExtension(base, "tgz", "tar")) return base;
	if (removeExtension(base, "gz")) return base;
	return base;
}
