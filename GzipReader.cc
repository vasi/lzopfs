#include "GzipReader.h"

#include "CompressedFile.h"
#include "GzipFile.h"

void GzipReader::throwEx(const std::string& s, int err) {
	if (err == Z_OK)
		return;
	throw Exception(zerr(s, err));
}

void GzipReader::resetWindow() {
	mStream.avail_out = mWindow.size();
	mStream.next_out = &mWindow[0];
}

void GzipReader::saveSwap(GzipReader& o) {	
	std::swap(mInput, o.mInput);
	std::swap(mWindow, o.mWindow);
	std::swap(mInitOutPos, o.mInitOutPos);
	std::swap(mOutBytes, o.mOutBytes);
	
	z_stream tmp = o.mStream;
	o.mStream = mStream;
	mStream = tmp;
	
	// Other fields shouldn't change
}

// Chunk and Window size aren't available in constructor, so do this
// separately
void GzipReader::setupBuffers() {
	if (!mInitialized) {
		mInput.resize(mChunkSize);
		mWindow.resize(mWindowSize);
		
		resetWindow();
		mStream.avail_in = 0;
		
		mInitialized = true;
	}
}

int GzipReader::step() {
	setupBuffers();
	if (mStream.avail_in == 0) {
		mStream.avail_in = mFH.tryRead(mInput, mChunkSize);
		mStream.next_in = &mInput[0];
	}
	if (mStream.avail_out == 0)
		resetWindow();
	
	mOutBytes += mStream.avail_out;
	int err = inflate(&mStream, Z_BLOCK);
	mOutBytes -= mStream.avail_out;
	return err;
}

int GzipReader::stepThrow() {
	int err = step();
	if (err != Z_STREAM_END)
		throwEx("inflate", err);
	return err;
}

int GzipReader::block() {
	int err;
	do {
		err = step();
		if (err != Z_OK && err != Z_STREAM_END)
			return err;
		if (mStream.data_type & 128)
			break;
	} while (err != Z_STREAM_END);
	return err;
}

GzipReader::GzipReader(const FileHandle& fh, Wrapper wrap)
		: mInitialized(false),
		mChunkSize(CompressedFile::ChunkSize),
		mWindowSize(GzipFile::WindowSize),
		mFH(fh),
		mInitOutPos(0), mOutBytes(0),
		mSave(0) {
	mStream.zfree = Z_NULL;
	mStream.zalloc = Z_NULL;
	mStream.opaque = Z_NULL;
	throwEx("inflate init", inflateInit2(&mStream, wrap));
}

GzipReader::~GzipReader() {
	inflateEnd(&mStream);
	if (mSave)
		delete mSave;
}

std::string GzipReader::zerr(const std::string& s, int err) const {
	char *w = NULL;	
	asprintf(&w, "%s: %s (%d)", s.c_str(), mStream.msg, err);
	std::string ws(w);
	free(w);
	return ws;
}

void GzipReader::save() {
	setupBuffers();
	if (!mSave) {
		mSave = new GzipReader(mFH);
		mSave->setupBuffers();
	}
	saveSwap(*mSave);
	mSaveSeek = mFH.tell();
	
	// Fixup state to correspond to where we were
	throwEx("inflateReset", inflateReset2(&mStream, Raw));
	
	mInput = mSave->mInput;
	mStream.avail_in = mSave->mStream.avail_in;
	mStream.next_in = &mInput[0] + mInput.size() - mStream.avail_in;
	
	size_t bits = mSave->ibits();
	if (bits)
		throwEx("prime", inflatePrime(&mStream, bits,
			mStream.next_in[-1] >> (8 - bits)));
	
	mInitOutPos = mSave->opos();
	mOutBytes = 0;
}

void GzipReader::restore() {
	if (!mSave)
		throw std::runtime_error("no saved state");
	
	saveSwap(*mSave);
	mFH.seek(mSaveSeek, SEEK_SET);
}

void GzipReader::copyWindow(Buffer& buf) {
	buf.resize(mWindow.size());
	std::rotate_copy(mWindow.begin(), mWindow.end() - mStream.avail_out,
		mWindow.end(), buf.begin());
}

void GzipHeaderReader::header(gz_header& hdr) {
	inflateGetHeader(&mStream, &hdr);
	while (!hdr.done)
		stepThrow();
}
