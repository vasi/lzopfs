#include "FileHandle.h"

#include <algorithm>
#include <cerrno>

#include <fcntl.h>
#include <unistd.h>

#define THROW_EX(_func) (throwEx(_func, errno))

void FileHandle::throwEx(const char *call, int err) const {
	std::string why(call);
	why = why + " error for file " + mPath;
	throw Exception(why, err);
}

FileHandle::FileHandle(const std::string& path, int flags, mode_t mode)
		: mFD(-1), mOwnFD(false) {
	open(path, flags, mode);
}

void FileHandle::open(const std::string& path, int flags, mode_t mode) {
	if (mFD != -1)
		throwEx("double open", 0);

	mPath = path;
	mFD = ::open(mPath.c_str(), flags, mode);
	if (mFD == -1) THROW_EX("open");
	mOwnFD = true;
}

FileHandle& FileHandle::operator=(const FileHandle& o) {
	close();
	mOwnFD = false;
	mPath = o.mPath;
	mFD = o.mFD;
	return *this;
}

void FileHandle::close() {
	if (mOwnFD && mFD != -1)
		::close(mFD);
	mFD = -1;
}

FileHandle::~FileHandle() {
	close();
}

void FileHandle::read(void *buf, size_t size) {
	if (tryRead(buf, size) < size)
		throw EOFException(mPath);
}

void FileHandle::read(Buffer& buf, size_t size) {
	buf.resize(size);
	read(&buf[0], size);
}

void FileHandle::pread(off_t off, void *buf, size_t size) const {
	if (tryPRead(off, buf, size) < size)
		throw EOFException(mPath);
}

void FileHandle::pread(off_t off, Buffer& buf, size_t size) const {
	buf.resize(size);
	pread(off, &buf[0], size);
}

size_t FileHandle::tryPRead(off_t off, Buffer& buf, size_t size) const {
	buf.resize(size);
	size_t bytes = tryPRead(off, &buf[0], size);
	buf.resize(bytes);
	return bytes;
}

size_t FileHandle::tryPRead(off_t off, void *buf, size_t size) const {
	ssize_t bytes = ::pread(mFD, buf, size, off);
	if (bytes < 0)
		THROW_EX("pread");
	if (bytes == 0)
		throw EOFException(mPath);
	return size_t(bytes);
}

size_t FileHandle::tryRead(Buffer& buf, size_t size) {
	buf.resize(size);
	size_t bytes = tryRead(&buf[0], size);
	buf.resize(bytes);
	return bytes;
}

size_t FileHandle::tryRead(void *buf, size_t size) {
	ssize_t bytes = ::read(mFD, buf, size);
	if (bytes < 0)
		THROW_EX("read");
	if (bytes == 0)
		throw EOFException(mPath);
	return size_t(bytes);
}

void FileHandle::write(const void *buf, size_t size) {
	ssize_t bytes = ::write(mFD, buf, size);
	if (bytes < 0 || size_t(bytes) < size)
		THROW_EX("write");
}

off_t FileHandle::seek(off_t offset, int whence) {
	off_t ret = ::lseek(mFD, offset, whence);
	if (ret == -1)
		THROW_EX("seek");
	return ret;
}

off_t FileHandle::tell() const {
	return const_cast<FileHandle*>(this)->seek(0, SEEK_CUR);
}

off_t FileHandle::size() const {
	off_t cur = tell();
	FileHandle* t = const_cast<FileHandle*>(this);
	t->seek(0, SEEK_END);
	off_t size = t->tell();
	t->seek(cur, SEEK_SET);
	return size;
}

#ifdef USE_BIG_ENDIAN
    #if USE_BIG_ENDIAN == 0
        #define __LITTLE_ENDIAN__
    #endif
#else
	#error "No endianness specified!"
#endif

void FileHandle::convertBEBuf(char *buf, size_t size) {
	#ifdef __LITTLE_ENDIAN__
		std::reverse(buf, buf + size);
	#endif
}

void FileHandle::convertLEBuf(char *buf, size_t size) {
	#ifndef __LITTLE_ENDIAN__
		std::reverse(buf, buf + size);
	#endif
}

void FileHandle::writeBuf(const Buffer& b, const std::string& path) {
	FileHandle fh(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	fh.write(b);
}
