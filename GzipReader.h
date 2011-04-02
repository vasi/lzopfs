#ifndef GZIPREADER_H
#define GZIPREADER_H

#include "lzopfs.h"
#include "FileHandle.h"

#include <zlib.h>

class GzipReader {
private:
	// Disable copying
	GzipReader(const GzipReader& o);
	GzipReader& operator=(const GzipReader& o);
	
public:
	static const size_t WindowSize;	
	
	struct Exception : std::runtime_error {
		Exception(const std::string& s) : std::runtime_error(s) { }
	};
	
	enum Wrapper {
		Gzip = 16 + MAX_WBITS,
		Raw = -MAX_WBITS,
	};
	enum Type { BareClone };

protected:
	bool mInitialized;
	size_t mChunkSize, mWindowSize;
	FileHandle mFH;
	
	Buffer mInput, mWindow;
	z_stream mStream;
	off_t mInitOutPos, mOutBytes;
	
	GzipReader *mSave;
	off_t mSaveSeek;
	
	void throwEx(const std::string& s, int err);
	void setupBuffers();
	void resetWindow();
	void saveSwap(GzipReader& o);
	
	int step();
	int stepThrow();
	
public:
	GzipReader(const FileHandle& fh, Wrapper wrap = Raw);
	~GzipReader();
	
	std::string zerr(const std::string& s, int err = Z_OK) const;
	
	void save();
	void restore();
	
	int block();
	
	off_t ipos() const { return mFH.tell() - mStream.avail_in; }
	off_t opos() const { return mInitOutPos + mOutBytes; }
	off_t obytes() const { return mOutBytes; }
	
	void save(GzipReader& o);
	void restore(GzipReader& o);
};

struct GzipHeaderReader : public GzipReader {
	GzipHeaderReader(const FileHandle& fh) : GzipReader(fh, Gzip)
		{ mChunkSize = 512; mWindowSize = 1; }
	void header(gz_header& hdr);
};

#endif // GZIPREADER_H
