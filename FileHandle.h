#ifndef FILEHANDLE_H
#define FILEHANDLE_H

#include "lzopfs.h"

#include <stdexcept>
#include <string>

#include <fcntl.h>

class FileHandle {
protected:
	std::string mPath;
	int mFD;
	bool mOwnFD;
	
	static void convertBEBuf(char *buf, size_t size);
	static void convertLEBuf(char *buf, size_t size);
	
	void throwEx(const char *call, int err) const;
	
public:
	struct Exception : public virtual std::runtime_error {
		int error_code;
		Exception(const std::string& s, int err)
			: std::runtime_error(s), error_code(err) { }
	};
	struct EOFException : public virtual std::runtime_error {
		EOFException(const std::string& s) : std::runtime_error(s) { }
	};
	
	static void writeBuf(const Buffer& b, const std::string& path);
	
	FileHandle(int fd = -1) : mFD(fd), mOwnFD(false) { }
	FileHandle(const std::string& path, int flags, mode_t mode = 0444);
	virtual ~FileHandle();
	
	void open(const std::string& path, int flags, mode_t mode = 0444);
	void close();
	
	FileHandle(const FileHandle& o) { *this = o; }
	FileHandle& operator=(const FileHandle& o);
	
	bool open() const { return mFD != -1; }
	
	void read(void *buf, size_t size);
	void read(Buffer& buf, size_t size);
	size_t tryRead(void *buf, size_t size);
	size_t tryRead(Buffer& buf, size_t size);
	
	void pread(off_t off, void *buf, size_t size) const;
	void pread(off_t off, Buffer& buf, size_t size) const;
	size_t tryPRead(off_t off, void *buf, size_t size) const;
	size_t tryPRead(off_t off, Buffer& buf, size_t size) const;
	
	void write(const void *buf, size_t size);
	void write(const Buffer& buf) { write(&buf[0], buf.size()); }
	off_t seek(off_t offset, int whence = SEEK_CUR);
	off_t tell() const;
	off_t size() const;
	
	template <typename T>
	static void convertBE(T &t) {
		convertBEBuf(reinterpret_cast<char*>(&t), sizeof(T));
	}
	template <typename T>
	static void convertLE(T &t) {
		convertLEBuf(reinterpret_cast<char*>(&t), sizeof(T));
	}
	
	template <typename T>
	void readBE(T& t) {
		read(&t, sizeof(T));
		convertBE(t);
	}
	template <typename T>
	void readLE(T& t) {
		read(&t, sizeof(T));
		convertLE(t);
	}
	
	template <typename T>
	void writeBE(T t) {
		convertBE(t);
		write(&t, sizeof(T));
	}
};

#endif	// FILEHANDLE_H
