#include "GzipReader.h"

#include "CompressedFile.h"
#include "GzipFile.h"

void GzipReaderBase::throwEx(const std::string& s, int err) {
	if (err == Z_OK)
		return;
	throw Exception(zerr(s, err));
}

void GzipReaderBase::setDict(const Buffer& dict) {
	initialize();
	if (!dict.empty())
		throwEx("setDict",
			inflateSetDictionary(&mStream, &dict[0], dict.size()));
}

void GzipReaderBase::prime(uint8_t byte, size_t bits) {
	initialize();
	if (bits != 0)
		throwEx("prime", inflatePrime(&mStream, bits, byte >> (8 - bits)));
}

int GzipReaderBase::step(int flush) {
	initialize();
	if (mStream.avail_in == 0) {
		mStream.avail_in = mFH.tryRead(mInput, chunkSize());
		mStream.next_in = &mInput[0];
	}
	if (mStream.avail_out == 0)
		writeOut(outBuf().size());
	
	mOutBytes += mStream.avail_out;
	int err = inflate(&mStream, flush);
	mOutBytes -= mStream.avail_out;
	return err;
}

int GzipReaderBase::stepThrow(int flush) {
	int err = step(flush);
	if (err != Z_STREAM_END)
		throwEx("inflate", err);
	return err;
}

void GzipReaderBase::initialize(bool force) {
	if (!force && mInitialized)
		return;
	
	throwEx("init", inflateInit2(&mStream, wrapper()));
	resetOutBuf();
	
	mInitialized = true;
}

size_t GzipReaderBase::chunkSize() const {
	return CompressedFile::ChunkSize;
}

GzipReaderBase::GzipReaderBase(FileHandle& fh)
		: mInitialized(false), mFH(fh), mOutBytes(0) {
	mStream.zfree = Z_NULL;
	mStream.zalloc = Z_NULL;
	mStream.opaque = Z_NULL;
	mStream.avail_in = 0;
}

void GzipReaderBase::swap(GzipReaderBase& o) {
	initialize();
	o.initialize();
	std::swap(mStream, o.mStream);
	std::swap(mInput, o.mInput);
	std::swap(mOutBytes, o.mOutBytes);
}

std::string GzipReaderBase::zerr(const std::string& s, int err) const {
	char *w = NULL;	
	asprintf(&w, "%s: %s (%d)", s.c_str(), mStream.msg, err);
	std::string ws(w);
	free(w);
	return ws;
}

int GzipReaderBase::block() {
	int err;
	do {
		err = step(Z_BLOCK);
		if (err != Z_OK && err != Z_STREAM_END)
			return err;
		if (mStream.data_type & 128)
			break;
	} while (err != Z_STREAM_END);
	return err;
}

void GzipReaderBase::reset(Wrapper w) {
	throwEx("inflateReset", inflateReset2(&mStream, w));
}

GzipBlockReader::GzipBlockReader(FileHandle& fh, Buffer& ubuf,
		const Block& b, const Buffer& dict, size_t bits)
		: GzipReaderBase(fh), mOutBuf(ubuf) {
	setDict(dict);
	mFH.seek(b.coff - (bits ? 1 : 0), SEEK_SET);
	if (bits) {
		uint8_t byte;
		mFH.readBE(byte);
		prime(byte, bits);
	}
}

void GzipBlockReader::read() {
	while (mStream.avail_out)
		stepThrow(Z_NO_FLUSH);
}

void PositionedGzipReader::skipFooter() {
	if (wrapper() == Gzip)
		return; // footer should've been processed
	const size_t footerSize = 8;
	if (mStream.avail_in < footerSize) {
		mFH.seek(footerSize - mStream.avail_in, SEEK_CUR);
		mStream.avail_in = 0;
	} else {
		mStream.avail_in -= footerSize;
		mStream.next_in += footerSize;
	}
}

size_t SavingGzipReader::windowSize() const {
	return GzipFile::WindowSize;
}

void SavingGzipReader::save() {
	if (!mSave)
		mSave = new PositionedGzipReader(mFH);
	swap(*mSave);
	
	mSaveSeek = mFH.tell();
	
	// Fixup state to correspond to where we were
	reset(Raw);
	
	mInput = mSave->mInput;
	mStream.avail_in = mSave->mStream.avail_in;
	mStream.next_in = &mInput[0] + mInput.size() - mStream.avail_in;
	
	mOutBuf.resize(windowSize());
	resetOutBuf();
	
	size_t bits = mSave->ibits();
	prime(mStream.next_in[-1], bits);
	
	mInitOutPos = mSave->opos();
	mOutBytes = 0;
}

void SavingGzipReader::restore() {
	if (!mSave)
		throw std::runtime_error("no saved state");
	
	swap(*mSave);
	mFH.seek(mSaveSeek, SEEK_SET);
}

void SavingGzipReader::copyWindow(Buffer& buf) {
	buf.resize(outBuf().size());
	std::rotate_copy(outBuf().begin(), outBuf().end() - mStream.avail_out,
		outBuf().end(), buf.begin());
}
