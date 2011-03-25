#include "lzopfs.h"
#include "BlockCache.h"

#include <cerrno>

#include <fuse.h>

namespace {

BlockCache *gBlockCache = 0;
const char *gSourcePath = 0;

extern "C" int lf_getattr(const char *path, struct stat *stbuf) {
	memset(stbuf, 0, sizeof(*stbuf));
	
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 3;
	} else if (strcmp(path, "/dest") == 0) {
		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
		stbuf->st_size = gBlockCache->file()->uncompressedSize();
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
	
	gBlockCache->read(buf, size, offset);
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
	
	LzopFile *lzop = new LzopFile(gSourcePath);
	gBlockCache = new BlockCache(lzop, 1024 * 1024 * 32);
	
	return fuse_main(args.argc, args.argv, &ops, NULL);	
}
