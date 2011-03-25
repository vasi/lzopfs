#include <fuse.h>
#include <lzo/lzo1x.h>

#include <algorithm>
#include <stdexcept>
#include <vector>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <unistd.h>

#include <libkern/OSByteOrder.h>

namespace {

typedef std::vector<uint8_t> Buffer;

struct Block {
	uint32_t usize, csize;
	uint64_t coff, uoff;
	
	Block(uint32_t us, uint32_t cs, uint32_t co, uint32_t uo)
		: usize(us), csize(cs), coff(co), uoff(uo) { }
};
typedef std::vector<Block> BlockList;

const char *gSourcePath = 0;
int gSourceFD;
BlockList gBlocks;
Buffer gCBuf, gUBuf;

const char LzopMagic[] = { 0x89, 'L', 'Z', 'O', '\0', '\r', '\n',
	0x1a, '\n' };

enum LzopFlags {
	AdlerDec	= 1 << 0,
	AdlerComp	= 1 << 1,
	CRCDec		= 1 << 8,
	CRCComp		= 1 << 9,
};

off_t uncompressedSize(const BlockList& blocks);
void lzopRead(int fd, void *buf, size_t size, off_t off,
	const BlockList& blocks, Buffer& cbuf, Buffer& ubuf);

extern "C" int lf_getattr(const char *path, struct stat *stbuf) {
	memset(stbuf, 0, sizeof(*stbuf));
	
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 3;
	} else if (strcmp(path, "/dest") == 0) {
		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
		stbuf->st_size = uncompressedSize(gBlocks);
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
	if (strcmp(path, "/dest") != 0)
		return -ENOENT;
	
	lzopRead(gSourceFD, buf, size, offset, gBlocks, gCBuf, gUBuf);
	return size;
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

void lzopReadBlocks(int fd, BlockList& blocks) {
	// Check magic
	char magic[sizeof(LzopMagic)];
	readEx(fd, magic, sizeof(magic));
	if (memcmp(magic, LzopMagic, sizeof(magic)) != 0)
		throw std::runtime_error("magic mismatch");
	
	// FIXME: Check headers for sanity
	uint32_t flags;
	// skip versions, method, level
	seekEx(fd, 3 * sizeof(uint16_t) + 2 * sizeof(uint8_t), SEEK_CUR);
	readBE(fd, flags);
	// skip mode, mtimes, filename, checksum
	seekEx(fd, 3 * sizeof(uint32_t) + sizeof(uint8_t)
		+ sizeof(uint32_t), SEEK_CUR);
	
	// How much space for checksums?
	size_t csums = 0, usums = 0;
	if (flags & CRCComp) ++csums;
	if (flags & AdlerComp) ++csums;
	if (flags & CRCDec) ++usums;
	if (flags & AdlerDec) ++usums;
	csums *= sizeof(uint32_t);
	usums *= sizeof(uint32_t);
	
	// Iterate thru the blocks
	size_t bheader = 2 * sizeof(uint32_t);
	uint32_t usize, csize;
	off_t uoff = 0, coff = xtell(fd);
	size_t sums;
	while (true) {
		readBE(fd, usize);
		if (usize == 0)
			break;
		readBE(fd, csize);
		
		sums = usums;
		if (usize != csize)
			sums += csums;
		
		blocks.push_back(Block(usize, csize,
			coff + bheader + sums, uoff));
		
		coff += sums + csize + 2 * sizeof(uint32_t);
		uoff += usize;
		seekEx(fd, sums + csize, SEEK_CUR);
	}
}

struct BlockOffsetOrdering {
	bool operator()(const Block& b, off_t off) {
		return (b.uoff + b.usize - 1) < (uint64_t)off;
	}
	
	bool operator()(off_t off, const Block& b) {
		return (uint64_t)off < b.uoff;
	}
};

BlockList::const_iterator findBlock(const BlockList& blocks,
		off_t off) {
	BlockList::const_iterator iter = std::lower_bound(
		blocks.begin(), blocks.end(), off, BlockOffsetOrdering());
	if (iter == blocks.end())
		throw std::runtime_error("can't find block");
	return iter;
}

void decompressBlock(int fd, const Block& b, Buffer& cbuf, Buffer &ubuf) {
	seekEx(fd, b.coff, SEEK_SET);
	cbuf.resize(b.csize);
	readEx(fd, &cbuf[0], b.csize);
	
	lzo_uint usize = b.usize;
	ubuf.resize(usize);
	fprintf(stderr, "Decompressing from %lld\n", b.coff);
	int err = lzo1x_decompress_safe(&cbuf[0], cbuf.size(), &ubuf[0],
		&usize, 0);
	if (err != LZO_E_OK) {
		fprintf(stderr, "lzo err: %d\n", err);
		throw std::runtime_error("decompression error");
	}
}

off_t uncompressedSize(const BlockList& blocks) {
	if (blocks.empty())
		return 0;
	const Block& b = blocks.back();
	return b.uoff + b.usize;
}

void lzopRead(int fd, void *buf, size_t size, off_t off,
		const BlockList& blocks, Buffer& cbuf, Buffer& ubuf) {
	BlockList::const_iterator biter = findBlock(blocks, off);
	while (size > 0) {
		decompressBlock(fd, *biter, cbuf, ubuf);
		size_t bstart = off - biter->uoff;
		size_t bsize = std::min(size, ubuf.size() - bstart);
		memcpy(buf, &ubuf[bstart], bsize);
		
		off += bsize;
		size -= bsize;
		++biter;
	}
}

} // anon namespace

int main(int argc, char *argv[]) {
	lzo_init();
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
	
	gSourceFD = open(gSourcePath, O_RDONLY);
	lzopReadBlocks(gSourceFD, gBlocks);
	
	return fuse_main(args.argc, args.argv, &ops, NULL);	
}
