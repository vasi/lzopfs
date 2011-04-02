#include "GzipFile.h"

#include "PathUtils.h"

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

void GzipFile::buildIndex(FileHandle& fh) {
	fh.seek(0, SEEK_SET);
	GzipReader rd(fh, GzipReader::Gzip);
	
	int err;
	bool rewind = false;
	do {
		err = rd.block();
		if (err != Z_OK && err != Z_STREAM_END) {
			if (rewind) {
				fprintf(stderr, "failure, rewinding!\n");
				rewind = false;
				rd.restore();
			} else {
				fprintf(stderr, "%s\n", rd.zerr("ERR", err).c_str());
				exit(1);
			}
		} else {
			fprintf(stderr, "block: in = %9lld, out = %9lld\n",
				rd.ipos(), rd.opos());
			if (rd.opos() == 128 * 1024) {
				fprintf(stderr, "saving!\n");
				rd.save();
				rewind = true;
			}
		}
	} while (err != Z_STREAM_END);
	
	fprintf(stderr, "got here!\n");
	exit(1);
	
#if 0
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
#endif
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
