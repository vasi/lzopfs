#include "lzopfs.h"

#include "BlockCache.h"
#include "FileList.h"
#include "LzopFile.h"
#include "OpenCompressedFile.h"

#include <cerrno>

#include <fuse.h>

namespace {

typedef uint64_t FuseFH;

const char *gNextSource = 0;
BlockCache gBlockCache;
FileList gFiles;

extern "C" int lf_getattr(const char *path, struct stat *stbuf) {
	memset(stbuf, 0, sizeof(*stbuf));
	
	LzopFile *file;
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 3;
	} else if ((file = gFiles.find(path))) {
		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
		stbuf->st_size = file->uncompressedSize();
	} else {
		return -ENOENT;
	}
	return 0;
}

struct DirFiller {
	void *buf;
	fuse_fill_dir_t filler;
	DirFiller(void *b, fuse_fill_dir_t f) : buf(b), filler(f) { }
	void operator()(const std::string& path) {
		filler(buf, path.c_str() + 1, NULL, 0);
	}
};

extern "C" int lf_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		off_t offset, struct fuse_file_info *fi) {
	if (strcmp(path, "/") != 0)
		return -ENOENT;
	
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	gFiles.forNames(DirFiller(buf, filler));
	return 0;
}

extern "C" int lf_open(const char *path, struct fuse_file_info *fi) {
	LzopFile *file;
	if (!(file = gFiles.find(path)))
		return -ENOENT;
	if ((fi->flags & O_ACCMODE) != O_RDONLY)
		return -EACCES;
	
	try {
		fi->fh = FuseFH(new OpenCompressedFile(file, fi->flags));
		return 0;
	} catch (FileHandle::Exception& e) {
		return e.error_code;
	}
}

extern "C" int lf_release(const char *path, struct fuse_file_info *fi) {
	delete reinterpret_cast<OpenCompressedFile*>(fi->fh);
	fi->fh = 0;
	return 0;
}

extern "C" int lf_read(const char *path, char *buf, size_t size, off_t offset,
		struct fuse_file_info *fi) {
	reinterpret_cast<OpenCompressedFile*>(fi->fh)->read(
		gBlockCache, buf, size, offset);
	return size;
}

extern "C" int lf_opt_proc(void *data, const char *arg, int key,
		struct fuse_args *outargs) {
	if (key == FUSE_OPT_KEY_NONOPT) {
		if (gNextSource)
			gFiles.add(gNextSource);
		gNextSource = arg;
		return 0;
	}
	return 1;
}

} // anon namespace

int main(int argc, char *argv[]) {
	try {
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
		if (gNextSource)
			fuse_opt_add_arg(&args, gNextSource);
		
		gBlockCache.maxSize(1024 * 1024 * 32);
		
		return fuse_main(args.argc, args.argv, &ops, NULL);
	} catch (std::runtime_error& e) {
		fprintf(stderr, "%s\n", e.what());
		return -1;
	}
}
