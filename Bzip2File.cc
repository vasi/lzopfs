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
		bm <<= sdiff * 8; // Clear high bytes
		bm >>= sdiff * 8;
		if (bm != BlockMagic)
			throwFormat("block magic");
	} catch (FileHandle::EOFException& e) {
		throwFormat("EOF");
	}
}

void Bzip2File::buildIndex(FileHandle& fh) {
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
