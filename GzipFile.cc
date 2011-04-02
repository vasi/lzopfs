#include "GzipFile.h"

#include "PathUtils.h"

const size_t GzipFile::WindowSize = 1 << MAX_WBITS; 

void GzipFile::setupStream(z_stream& s) {
	s.zalloc = Z_NULL;
	s.zfree = Z_NULL;
	s.opaque = Z_NULL;
}

void GzipFile::ensureInput(z_stream& s, FileHandle& fh) {
	if (s.avail_in == 0) {
		s.avail_in = fh.tryRead(mInBuf, ChunkSize);
		s.next_in = &mInBuf[0];
	}
}

bool GzipFile::ensureOutput(z_stream& s, Buffer& w, bool force) {
	if (!force && s.avail_out)
		return false;
	s.next_out = &w[0];
	s.avail_out = w.size();
	return true;
}

void GzipFile::checkFileType(FileHandle& fh) {
	try {
		// Read til end of header
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
			ensureInput(mStream, fh);
			if (inflate(&mStream, Z_BLOCK) != Z_OK)
				throwFormat("can't read gzip header");
		}		
	} catch (FileHandle::EOFException& e) {
		throwFormat("EOF");
	}
}

void GzipFile::buildIndex(FileHandle& fh) {
	Buffer window(WindowSize);
	ensureOutput(mStream, window, true);
	
	// For independent inflating
	enum RewindStatus { NoRewind, CanRewind, DidRewind };
	RewindStatus rewind = NoRewind;
	off_t saveOut, saveIn;
	Buffer saveWin(window.size());
	
	z_stream s2;
	setupStream(s2);
	if (inflateInit2(&s2, -MAX_WBITS) != Z_OK)
		throwFormat("can't initialize secondary stream");
	
	z_stream *s = &mStream, *so = &s2;
	off_t out = 0; // bytes discarded
	int err;
	do {
		if (s->data_type & 128) { // Block boundary!
			if (rewind == CanRewind && s->total_out >= WindowSize) {
				fprintf(stderr, "\t\t\t\t\t\t\t\tsuccess!\n");
				rewind = NoRewind;
			}
			
			off_t ib = fh.tell() - s->avail_in,
				ob = out + window.size() - s->avail_out;
			size_t bits = (s->data_type & 7);
			fprintf(stderr, "block: in = %lld, out = %lld, bits = %zu\n",
				ib, ob, bits);
			
			if (rewind == DidRewind) {
				rewind = NoRewind; // failed, just read normally
			} else if (rewind == NoRewind) {
				if (bits == 0) { // Try to read independently
					fprintf(stderr, "trying independent read...\n");
					
					rewind = CanRewind;
					saveIn = ib;
					saveOut = out;
					saveWin = window;
					std::swap(s, so);
					s->avail_in = so->avail_in;
					s->next_in = so->next_in;
					s->avail_out = so->avail_out;
					s->next_out = &window[window.size() - s->avail_out];
					s->total_out = 0;
					if (inflateReset2(s, -MAX_WBITS) != Z_OK)
						throwFormat("can't initialize secondary stream");
				}
			}
		}
		
		ensureInput(*s, fh);
		if (ensureOutput(*s, window))
			out += window.size();
				
		err = inflate(s, Z_BLOCK);
		if (err != Z_OK && err != Z_STREAM_END) {
			if (rewind == CanRewind) {
				fprintf(stderr, "\t\t\t\t\t\t\t\tfailure, rewinding\n");
				fh.seek(saveIn, SEEK_SET);
				out = saveOut;
				std::swap(saveWin, window);
				std::swap(s, so);
				s->avail_in = 0;
				rewind = DidRewind;
			} else {
				fprintf(stderr, "inflate: %d\n", err);
				throwFormat("can't inflate while indexing");
			}
		}
	} while (err != Z_STREAM_END);
	
	// FIXME: cleanup
	fprintf(stderr, "got here!\n"); exit(1);
}

GzipFile::GzipFile(const std::string& path, uint64_t maxBlock)
		: IndexedCompFile(path) {
	setupStream(mStream);
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
