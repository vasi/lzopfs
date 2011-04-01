#ifndef LZOPFILE_H
#define LZOPFILE_H

#include "lzopfs.h"
#include "CompressedFile.h"

class LzopFile : public IndexedCompFile {
protected:
	typedef uint32_t Checksum;
	
	enum Flags {
		AdlerDec	= 1 << 0,
		AdlerComp	= 1 << 1,
		ExtraField	= 1 << 6,
		CRCDec		= 1 << 8,
		CRCComp		= 1 << 9,
		MultiPart	= 1 << 10,
		Filter		= 1 << 11,
		HeaderCRC	= 1 << 12,
	};
	
	enum ChecksumType { Adler, CRC };
	
	static const char Magic[];
	static const uint16_t LzopDecodeVersion;
	
	
	typedef std::vector<Block> BlockList;
	BlockList mBlocks;
	uint32_t mFlags;
	
	class Iterator : public BlockIteratorInner {
		BlockList::const_iterator mIter, mEnd;
	public:
		Iterator(BlockList::const_iterator i, BlockList::const_iterator e)
			: mIter(i), mEnd(e) { }
		virtual void incr() { ++mIter; }
		virtual const Block& deref() const { return *mIter; }
		virtual bool end() const { return mIter == mEnd; }
		virtual BlockIteratorInner *dup() const
			{ return new Iterator(mIter, mEnd); }
	};
	
	
	void parseHeaders(FileHandle& fh, uint32_t& flags);
	Checksum checksum(ChecksumType type, const Buffer& buf);
	
	virtual void checkFileType(FileHandle &fh);
	virtual bool readIndex(FileHandle& fh);
	virtual void buildIndex(FileHandle& fh);
	virtual void writeIndex(FileHandle& fh) const;
	
public:
	static CompressedFile* open(const std::string& path, uint64_t maxBlock)
		{ return new LzopFile(path, maxBlock); }
	
	LzopFile(const std::string& path, uint64_t maxBlock);
	
	virtual std::string destName() const;
	
	virtual BlockIterator findBlock(off_t off) const;
	virtual void decompressBlock(FileHandle& fh, const Block& b,
		Buffer& ubuf);
	
	virtual off_t uncompressedSize() const;
};

#endif // LZOPFILE_H
