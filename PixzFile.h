#ifndef PIXZFILE_H
#define PIXZFILE_H

#include "lzopfs.h"
#include "CompressedFile.h"

#include <lzma.h> // FIXME: configure

class PixzFile : public CompressedFile {
protected:	
	class Iterator : public BlockIteratorInner {
		lzma_index_iter *mIter;
		Block mBlock;
		bool mEnd;
		
	public:
		Iterator(lzma_index_iter *i) : mIter(i), mEnd(false) {
			makeBlock();
		}
		virtual ~Iterator() { delete mIter; }
		
		virtual void makeBlock() {
			mBlock.coff = mIter->block.compressed_file_offset;
			mBlock.uoff = mIter->block.uncompressed_file_offset;
			mBlock.csize = mIter->block.total_size;
			mBlock.usize = mIter->block.uncompressed_size;			
		}
		
		virtual void incr() {
			if (mEnd || lzma_index_iter_next(
					mIter, LZMA_INDEX_ITER_NONEMPTY_BLOCK))
				mEnd = true;
			else
				makeBlock();
		}
		
		virtual const Block& deref() const { return mBlock; }
		virtual bool end() const { return mEnd; }
	};
	
	
	static const uint64_t MemLimit;
	static const size_t ChunkSize;
	
	
	lzma_index *mIndex;
	lzma_stream_flags mFlags;
	
	lzma_stream mStream;
	Buffer mBuf;
	
	
	lzma_ret code(FileHandle& fh);
		
public:
	PixzFile(const std::string& path);
	virtual ~PixzFile();
	
	// FIXME! .tpxz
	virtual std::string suffix() const { return ".pxz"; }
	
	virtual BlockIterator findBlock(off_t off) const;
	
	virtual void decompressBlock(FileHandle& fh, const Block& b,
		Buffer& ubuf);
	
	virtual off_t uncompressedSize() const;
};

#endif // PIXZFILE_H
