


CXXFLAGS=   -Wall -g -D_FILE_OFFSET_BITS=64
LDFLAGS =   -lfuse -pthread -lbz2 -lz -llzma -llzo2



all: lzopfs

lzopfs:	BlockCache.o Bzip2File.o CompressedFile.o FileHandle.o FileList.o GzipFile.o GzipReader.o LzopFile.o OpenCompressedFile.o PathUtils.o PixzFile.o ThreadPool.o
	g++ \
		$(CXXFLAGS) \
		-o lzopfs \
		$(LDFLAGS) \
		BlockCache.o Bzip2File.o CompressedFile.o FileHandle.o FileList.o GzipFile.o GzipReader.o LzopFile.o OpenCompressedFile.o PathUtils.o PixzFile.o ThreadPool.o \
		lzopfs.cc

BlockCache.o:
	g++ -c -g  BlockCache.cc $(CXXFLAGS)

Bzip2File.o:
	g++ -c -g  Bzip2File.cc $(CXXFLAGS)

CompressedFile.o:
	g++ -c -g  CompressedFile.cc $(CXXFLAGS)

FileHandle.o:
	g++ -c -g  FileHandle.cc $(CXXFLAGS)

FileList.o:
	g++ -c -g  FileList.cc $(CXXFLAGS)

GzipFile.o:
	g++ -c -g  GzipFile.cc $(CXXFLAGS)

GzipReader.o:
	g++ -c -g  GzipReader.cc $(CXXFLAGS)

LzopFile.o:
	g++ -c -g  LzopFile.cc $(CXXFLAGS)

OpenCompressedFile.o:
	g++ -c -g  OpenCompressedFile.cc $(CXXFLAGS)

PathUtils.o:
	g++ -c -g  PathUtils.cc $(CXXFLAGS)

PixzFile.o:
	g++ -c -g  PixzFile.cc $(CXXFLAGS)

ThreadPool.o:
	g++ -c -g  ThreadPool.cc $(CXXFLAGS)

#lzopfs.o: BlockCache.o Bzip2File.o CompressedFile.o FileHandle.o FileList.o GzipFile.o GzipReader.o LzopFile.o OpenCompressedFile.o PathUtils.o PixzFile.o ThreadPool.o
#	g++ -c -g  lzopfs.cc $(CXXFLAGS)



clean:
	rm lzopfs *.o
