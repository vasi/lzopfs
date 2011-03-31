#ifndef LZOPFILE_H
#define LZOPFILE_H

#include "lzopfs.h"
#include "FileHandle.h"

#include <string>

class LzopFile {
public:
	typedef std::vector<Block> BlockList;
	typedef BlockList::const_iterator BlockIterator;

	struct FormatException : public virtual std::runtime_error {
		std::string file;
		FormatException(const std::string& f, const std::string& s)
			: std::runtime_error(s), file(f) { }
		~FormatException() throw() { }
	};

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
	
	
	std::string mPath;
	BlockList mBlocks;
	
	void parseHeader(FileHandle& fh, uint32_t& flags);
	Checksum checksum(ChecksumType type, const Buffer& buf);
	void throwFormat(const std::string& s) const;
	
	void parseBlocks();
	std::string indexPath() const;
	bool readIndex();
	void writeIndex() const;
	
public:
	LzopFile(const std::string& path);
	
	const std::string& path() const { return mPath; }
	std::string destName() const;
	
	BlockIterator findBlock(off_t off) const;
	BlockIterator blockEnd() const { return mBlocks.end(); }
	
	void decompressBlock(FileHandle& fh, const Block& b, Buffer& ubuf);
	
	off_t uncompressedSize() const;
};

#endif // LZOPFILE_H
