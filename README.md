# lzopfs

This FUSE filesystem allows you to view a compressed file as if it was uncompressed, including random access operations. This can help use tools that don't understand compression, even on compressed files.

For example, if you have a gzip-compressed disk image, you can't loopback mount it directly. But something like this would work:

```
$ mkdir uncompressed-view mnt
$ lzopfs myimage.gz uncompressed-view
$ mount -o loop uncompressed-view/mydisk mnt
$ cat mnt/some-file
```

This doesn't actually uncompress your whole myimage.gz onto disk! Instead, only the parts that are needed will be decompressed at any particular moment. The total uncompressed size could even be bigger than your available disk space.

## How do I build it?

This works on Mac and Linux, you could almost certainly port it to FreeBSD or something too.

You'll need a bunch of dependencies installed. On Ubuntu, something like `apt install zlib1g-dev liblzo2-dev liblzma-dev libbz2-dev scons libfuse-dev`.

Then just run `scons` and you're good to go.

## How to I use it?

Run it like: `lzopfs file1.gz file2.bz2 ... mountpoint`

The mountpoint must already exist. For each file in the argument list, a corresponding decompressed synthetic file will be usable in the mountpoint, with the compression suffix removed.

Only a couple of options are supported:

* `--index-root=DIR`. For some formats, lzopfs needs to create auxiliary index files. It tries to put them next to the input files, but sometimes that's not great, such as if that's a read-only disk. This option tells lzopfs to put them somewhere else.

* `--block-factor=SCALE`. Gzip input files can require rather large auxiliary index files. This option tunes just how large they'll be: the larger SCALE is, the smaller index files you'll have, but the more expensive random access will be. The default is 32.

## What compression formats are supported?

For a compression format to work, it must be possible to do random access within it. The following formats are supported, in order of most- to least-preferred:

### xz (multi-block only)

For xz files produced by [pixz](https://github.com/vasi/pixz) or `xz -T`, there's an already an internal index that allows random access. Perfect for lzopfs!

Note that single-block xz files, the default produced by xz, are no good for lzopfs! They're one giant block, with no opportunity for random access.

### zstd (multi-frame only)

For zstd files using the [seekable format](https://github.com/facebook/zstd/blob/dev/contrib/seekable_format/zstd_seekable_compression_format.md), there's also an internal index that allows random access.

Unfortunately, the standard zstd command can't produce these files. You'll need to use something like [zstdseek](https://github.com/SaveTheRbtz/zstd-seekable-format-go) or [t2sz](https://github.com/martinellimarco/t2sz).

### lzop

[Lzop](https://www.lzop.org/) files don't have an internal index, but they do have blocks. To allow random access, lzopfs has to build its own index of which block corresponds to which uncompressed offset.

This involves doing a single scan of the lzop file, and writing a `.blockIdx` file. Once this file exists, lzopfs is happy to reuse it, without re-scanning the file each time.

Thankfully, lzop blocks have headers that include their length, so scanning lzop files is very fast.

### bzip2

Bzip2 also doesn't have an internal index, and its blocks don't have proper headers. Worse, its blocks don't always start on byte boundaries, but can start or end at any bit.

To index these files, lzopfs must look for bit sequences that look like they might be bzip2 blocks, and then test them all out by actually decompressing them. That's quite expensive, so indexing will be slow.

Even after the index is built, decompressing blocks requires repairing byte alignment, so it's also more expensive than the above formats.

### gzip

Gzip is the worst format for lzopfs. Even if you know where a gzip block begins, that's not enough to decompress it--you also need the current state of the DEFLATE decompressor.

Indexing gzip files requires actually decompressing them, and saving the DEFLATE "dictionary" every so often so that random access is possible. This can make index files for gzip quite large, up to 10% of the compressed file size. You can use the `--block-factor` option to tune this.

Lzopfs tries to be smart about this, and will prefer block boundaries that don't require a dictionary. So prefer gzip compressors that synchronize every so often. One way to achieve this is the use the `--rsyncable` option in most versions of gzip.

## Bugs

There are a lot! Don't expect error conditions to be handled properly.

## License

lzopfs was written by Dave Vasilevsky, and is available under a BSD-style license. See COPYING.