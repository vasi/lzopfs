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
	
	void throwEx(const char *call) const;
	
public:
	struct Exception : public virtual std::runtime_error {
		Exception(const std::string& s) : std::runtime_error(s) { }
	};
	struct EOFException : public virtual Exception {
		EOFException(const std::string& s)
			: std::runtime_error(s), Exception(s) { }
	};
	
	FileHandle(int fd = -1) : mFD(fd), mOwnFD(false) { }
	FileHandle(const std::string& path, int flags, mode_t mode = 0444);
	void open(const std::string& path, int flags, mode_t mode = 0444);
	virtual ~FileHandle();
	
	void read(void *buf, size_t size);
	void read(Buffer& buf, size_t size);
	void write(void *buf, size_t size);
	off_t seek(off_t offset, int whence);
	off_t tell() { return seek(0, SEEK_CUR); }
	
	template <typename T>
	static void convertBE(T &t) {
		convertBEBuf(reinterpret_cast<char*>(&t), sizeof(T));
	}
	
	template <typename T>
	void readBE(T& t) {
		read(&t, sizeof(T));
		convertBE(t);
	}
	
	template <typename T>
	void writeBE(T t) {
		convertBE(t);
		write(&t, sizeof(T));
	}
};

#endif	// FILEHANDLE_H
