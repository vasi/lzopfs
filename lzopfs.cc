#include <fuse.h>

#include <algorithm>
#include <stdexcept>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <unistd.h>

#include <libkern/OSByteOrder.h>

namespace {

const char *gSourcePath = 0;
int gSourceFD;

const char LzopMagic[] = { 0x89, 'L', 'Z', 'O', '\0', '\r', '\n',
	0x1a, '\n' };

enum LzopFlags {
	AdlerDec	= 1 << 0,
	AdlerComp	= 1 << 1,
	CRCDec		= 1 << 8,
	CRCComp		= 1 << 9,
};

extern "C" int lf_getattr(const char *path, struct stat *stbuf) {
	memset(stbuf, 0, sizeof(*stbuf));
	
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 3;
	} else if (strcmp(path, "/dest") == 0) {
		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
		stbuf->st_size = 1024 * 1024; // FIXME
	} else {
		return -ENOENT;
	}
	return 0;
}

extern "C" int lf_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		off_t offset, struct fuse_file_info *fi) {
	if (strcmp(path, "/") != 0)
		return -ENOENT;
	
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	filler(buf, "dest", NULL, 0);
	return 0;
}

extern "C" int lf_open(const char *path, struct fuse_file_info *fi) {
	if (strcmp(path, "/dest") != 0)
		return -ENOENT;
	
	if ((fi->flags & O_ACCMODE) != O_RDONLY)
		return -EACCES;
	return 0;
}

extern "C" int lf_release(const char *path, struct fuse_file_info *fi) {
	close(fi->fh);
	return 0;
}

extern "C" int lf_read(const char *path, char *buf, size_t size, off_t offset,
		struct fuse_file_info *fi) {
	return pread(gSourceFD, buf, size, offset); // FIXME
}

extern "C" int lf_opt_proc(void *data, const char *arg, int key,
		struct fuse_args *outargs) {
	if (gSourcePath == 0 && key == FUSE_OPT_KEY_NONOPT) {
		gSourcePath = arg;
		return 0;
	}
	return 1;
}

template <typename T>
void convertBE(T &t) {
	#ifdef __LITTLE_ENDIAN__
		char *p = reinterpret_cast<char*>(&t);
		std::reverse(p, p + sizeof(T));
	#endif
}

void readEx(int fd, void *buf, size_t size) {
	ssize_t bytes = read(fd, buf, size);
	if (bytes < 0 || (size_t)bytes != size)
		throw std::runtime_error("read failure");
}

void seekEx(int fd, off_t off, int whence) {
	off_t ret = lseek(fd, off, whence);
	if (ret == -1)
		throw std::runtime_error("seek failure");
}

off_t xtell(int fd) {
	off_t off = lseek(fd, 0, SEEK_CUR);
	if (off == -1)
		throw std::runtime_error("tell failure");
	return off;
}

template <typename T>
void readBE(int fd, T& t) {
	readEx(fd, &t, sizeof(T));
	convertBE(t);
}

} // anon namespace

int main(int argc, char *argv[]) {
	umask(0);
	
	struct fuse_operations ops;
	memset(&ops, 0, sizeof(ops));
	ops.getattr = lf_getattr;
	ops.readdir = lf_readdir;
	ops.open = lf_open;
	ops.release = lf_release;
	ops.read = lf_read;
	
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	fuse_opt_parse(&args, NULL, NULL, lf_opt_proc);
	
	
	// Check magic
	int gSourceFD = open(gSourcePath, O_RDONLY);
	char magic[sizeof(LzopMagic)];
	readEx(gSourceFD, magic, sizeof(magic));
	if (memcmp(magic, LzopMagic, sizeof(magic)) != 0)
		throw std::runtime_error("magic mismatch");
	
	// FIXME: Check headers for sanity
	uint32_t flags;
	// skip versions, method, level
	seekEx(gSourceFD, 3 * sizeof(uint16_t) + 2 * sizeof(uint8_t), SEEK_CUR);
	readBE(gSourceFD, flags);
	// skip mode, mtimes, filename, checksum
	seekEx(gSourceFD, 3 * sizeof(uint32_t) + sizeof(uint8_t)
		+ sizeof(uint32_t), SEEK_CUR);
	
	// How many checksums do we need?
	size_t csums = 0, usums = 0;
	if (flags & CRCComp) ++csums;
	if (flags & AdlerComp) ++csums;
	if (flags & CRCDec) ++usums;
	if (flags & AdlerDec) ++usums;
	
	// Iterate thru the blocks
	uint32_t usize, csize;
	off_t uoff = 0, coff = xtell(gSourceFD) + 2 * sizeof(uint32_t);
	size_t sums;
	while (true) {
		readBE(gSourceFD, usize);
		if (usize == 0)
			break;
		readBE(gSourceFD, csize);
		
		sums = usums;
		if (usize != csize)
			sums += csums;
		sums *= sizeof(uint32_t);
		
		fprintf(stderr, "uoff = %9lld, coff = %9lld, usize = %9u, "
			"csize = %9u\n", uoff, coff, usize, csize);
		
		coff += sums + csize + 2 * sizeof(uint32_t);
		uoff += usize;
		seekEx(gSourceFD, sums + csize, SEEK_CUR);
	}
	
//	return fuse_main(args.argc, args.argv, &ops, NULL);	
}
