#include "FileHandle.h"

#include <libkern/OSByteOrder.h>

void FileHandle::throwEx(const char *call) const {
	std::string why(call);
	why = why + " error for file " + mPath;
	throw Exception(why);
}

void FileHandle::convertBEBuf(char *buf, size_t size) {
	#ifdef __LITTLE_ENDIAN__
		std::reverse(buf, buf + size);
	#endif	
}

FileHandle::FileHandle(const std::string& path, int flags, mode_t mode)
		: mFD(-1), mOwnFD(false) {
	open(path, flags, mode);
}

void FileHandle::open(const std::string& path, int flags, mode_t mode) {
	if (mFD != -1)
		throwEx("double open");
	
	mPath = path;
	mFD = ::open(mPath.c_str(), flags, mode);
	if (mFD == -1) throwEx("open");
	mOwnFD = true;
}

FileHandle::~FileHandle() {
	if (mOwnFD && mFD != -1)
		::close(mFD);
}

void FileHandle::read(void *buf, size_t size) {
	ssize_t bytes = ::read(mFD, buf, size);
	if (bytes < 0)
		throwEx("read");
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
		throwEx("write");
}

off_t FileHandle::seek(off_t offset, int whence) {
	off_t ret = ::lseek(mFD, offset, whence);
	if (ret == -1)
		throwEx("seek");
	return ret;
}
