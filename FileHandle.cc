#include "FileHandle.h"

#include <algorithm>
#include <cerrno>

#include <fcntl.h>

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

FileHandle::~FileHandle() {
	if (mOwnFD && mFD != -1)
		::close(mFD);
}

void FileHandle::read(void *buf, size_t size) {
	ssize_t bytes = ::read(mFD, buf, size);
	if (bytes < 0)
		THROW_EX("read");
	if (size_t(bytes) < size)
		throw EOFException(mPath);
}

void FileHandle::read(Buffer& buf, size_t size) {
	buf.resize(size);
	read(&buf[0], size);
}

void FileHandle::write(void *buf, size_t size) {
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

off_t FileHandle::tell() {
	return seek(0, SEEK_CUR);
}


#include "config.h"
#ifdef HAVE_LIBKERN_OSBYTEORDER_H
	#include <libkern/OSByteOrder.h>
#elif defined(HAVE_ENDIAN_H)
	#include <endian.h>
	#if __BYTE_ORDER == __LITTLE_ENDIAN
		#define __LITTLE_ENDIAN__
	#endif
#else
	#error "No endianness header."
#endif

void FileHandle::convertBEBuf(char *buf, size_t size) {
	#ifdef __LITTLE_ENDIAN__
		std::reverse(buf, buf + size);
	#endif	
}

