#include "GzipFile.h"

#include "PathUtils.h"

const size_t GzipFile::WindowSize = 1 << MAX_WBITS; 
const uint64_t GzipFile::MaxDictBlockSize = 32 * WindowSize; 

void GzipFile::checkFileType(FileHandle& fh) {
	try {
		GzipHeaderReader rd(fh);
		gz_header hdr;
		rd.header(hdr);
	} catch (GzipReader::Exception& e) {
		throwFormat(e.what());
	} catch (FileHandle::EOFException& e) {
		throwFormat("EOF");
	}
}

#include <typeinfo>

void GzipFile::buildIndex(FileHandle& fh) {
try {
	fh.seek(0, SEEK_SET);
	GzipReader rd(fh, GzipReader::Gzip);
	
	int err;
	bool indep = false;
	off_t bin, bout;
	do {
		err = rd.block();
		if (err != Z_OK && err != Z_STREAM_END) {
			if (indep) { // can't decode this one, rewind!
				rd.restore();
				indep = false;
			} else {
				fprintf(stderr, "%s\n", rd.zerr("ERR", err).c_str());
				exit(1);
			}
		} else {
			if (indep && rd.obytes() > WindowSize) { // it's good!
				fprintf(stderr, "indep block: in = %9lld, out = %9lld\n",
					bin, bout);
				indep = false;
			}
			if (!indep) { // check if this block can be independently decoded
				bin = rd.ipos();
				bout = rd.opos();
				rd.save();
				indep = true;
			}
		}
	} while (err != Z_STREAM_END);
	fprintf(stderr, "end: in = %9lld, out = %9lld\n", rd.ipos(), rd.opos());
	exit(1);
} catch (std::runtime_error& e) {
	fprintf(stderr, "%s: %s\n", typeid(e).name(), e.what());
	throw;
}
}

GzipFile::GzipFile(const std::string& path, uint64_t maxBlock)
		: IndexedCompFile(path) {
	initialize(maxBlock);
}

bool GzipFile::readIndex(FileHandle& fh) {
	return false; // FIXME
	
	// FIXME: on success, close mStream, we won't need it
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
