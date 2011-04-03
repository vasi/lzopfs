#include "Bzip2File.h"

#include "PathUtils.h"

const char Bzip2File::Magic[3] = { 'B', 'Z', 'h' };

void Bzip2File::checkFileType(FileHandle& fh) {
	try {
		Buffer buf;
		fh.read(buf, sizeof(Magic));
		if (!std::equal(buf.begin(), buf.end(), Magic))
			throwFormat("magic");
		
		char level;
		fh.readBE(level);
		if (level < '1' || level > '9')
			throwFormat("invalid level");
		
		uint64_t bm;
		int sdiff = sizeof(BlockMagic) - BlockMagicBytes;
		fh.seek(-sdiff, SEEK_CUR);
		fh.readBE(bm);
		if ((bm & BlockMagicMask) != BlockMagic)
			throwFormat("block magic");
	} catch (FileHandle::EOFException& e) {
		throwFormat("EOF");
	}
}

void Bzip2File::findBlockBoundaryCandidates(FileHandle& fh) {
	// Find block boundary candidates
	try {
		Buffer buf;
		Buffer::const_iterator iter = buf.end();
		
		fh.seek(0, SEEK_SET);
		uint64_t test;
		fh.readBE(test); // ok if we skip the first couple bytes
		
		off_t pos = fh.tell() - BlockMagicBytes;
		size_t bits = 0; // how many bits from prev byte?
		uint8_t byte;
		while (true) {
			if ((test & BlockMagicMask) == BlockMagic)
				addBlock(new Bzip2Block(pos, bits));
			
			if (bits == 0) {
				++pos;
				if (iter == buf.end()) {
					fh.tryRead(buf, CompressedFile::ChunkSize);
					iter = buf.begin();
				}
				byte = *iter++;
			}
			bits = (bits + 7) % 8; // sub 1
			test = (test << 1) | (1 & (byte >> bits));
		}
	} catch (FileHandle::EOFException& e) {
		return;
	}	
}

void Bzip2File::buildIndex(FileHandle& fh) {
	findBlockBoundaryCandidates(fh);
	dumpBlocks();
	exit(-1);
}

void Bzip2File::decompressBlock(FileHandle& fh, const Block& b, Buffer& ubuf) {
}

std::string Bzip2File::destName() const {
	using namespace PathUtils;
	std::string base = basename(path());
	if (replaceExtension(base, "tbz2", "tar")) return base;
	if (removeExtension(base, "bz2")) return base;
	return base;
}
