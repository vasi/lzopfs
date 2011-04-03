#include "GzipFile.h"

#include "PathUtils.h"

const size_t GzipFile::WindowSize = 1 << MAX_WBITS; 
const uint64_t GzipFile::MinDictBlockSize = 32 * WindowSize; 

void GzipFile::checkFileType(FileHandle& fh) {
	try {
		GzipHeaderReader rd(fh);
		gz_header hdr;
		rd.header(hdr);
	} catch (GzipReaderBase::Exception& e) {
		throwFormat(e.what());
	} catch (FileHandle::EOFException& e) {
		throwFormat("EOF");
	}
}

// Fixup the prev block
void GzipFile::setLastBlockSize(off_t uoff, off_t coff) {
	if (mBlocks.empty())
		return;
	Block* p = mBlocks.back();
	p->usize = uoff - p->uoff;
	p->csize = coff - p->coff;
}

Buffer& GzipFile::addBlock(off_t uoff, off_t coff, size_t bits) {
	setLastBlockSize(uoff, coff);
	GzipBlock *b = new GzipBlock(uoff, coff, bits);
	IndexedCompFile::addBlock(b);
	return b->dict;
}

void GzipFile::buildIndex(FileHandle& fh) {
	fh.seek(0, SEEK_SET);
	SavingGzipReader rd(fh);
	
	/* Go through every block in the file.
	 * For each block, try to see first if it can be decoded independently,
	 * without a dictionary.
	 * Otherwise, when we've gone far enough, just give up and save the
	 * dictionary window. */
	bool indep = false;	// Are we trying an independent decode?
	bool backtrack = false; // Have we passed a block we may want?
	off_t uoff, coff, bits;	// Info of block we're currently examining
	off_t lastIdx = 0;		// Uncompressed pos of last indexed block
	
	int err;
	do {
		err = rd.block();
		if (err == Z_OK || err == Z_STREAM_END) {
			if (indep) {
				if (rd.obytes() > WindowSize) { // Yay, uoff block is indep!
//					fprintf(stderr, "...ok, adding!\n");
					addBlock(uoff, coff, bits);
					lastIdx = uoff;
					indep = false;
					if (backtrack) {
						rd.restore();
						continue; // Go to next block after uoff
					}
					// If !backtrack, continue from where we're at 
				} else if (rd.obytes() == 0) { // Zero-length block, ignore
//					fprintf(stderr, "zero, rewinding!\n");
					indep = false;
					rd.restore();
					continue;
				} else {
//					fprintf(stderr, "(backtrack)\n");
					// Mark that there's a block between uoff and window end
					backtrack = true;
				}
			}
			
			if (!indep) { // Check if this new block is indep
				uoff = rd.opos();
				coff = rd.ipos();
				bits = rd.ibits();
//				fprintf(stderr, "Checking %lld, %lld\n", uoff, coff);
				rd.save();
				backtrack = false;
				indep = true;
			}
		} else { // There was an error
			if (!indep)
				throwFormat(rd.zerr("gzip decode", err));
			
			if (indep) { // Indep decode failed, so rewind
//				fprintf(stderr, "...failed, rewinding!\n");
				indep = false;
				rd.restore();
				if (rd.opos() - lastIdx > MinDictBlockSize) {
					// Add a dict block
//					fprintf(stderr, "Dict block\n");
					Buffer& dict = addBlock(rd.opos(), rd.ipos(), rd.ibits());
					rd.copyWindow(dict);
					lastIdx = rd.opos();
				}
			} 
		}
	} while (err != Z_STREAM_END);
	setLastBlockSize(rd.opos(), rd.ipos());	
//	dumpBlocks();
}

GzipFile::GzipFile(const std::string& path, uint64_t maxBlock)
		: IndexedCompFile(path) {
	initialize(maxBlock);
}

void GzipFile::decompressBlock(FileHandle& fh, const Block& b, Buffer& ubuf) {
	const GzipBlock& gb = dynamic_cast<const GzipBlock&>(b);
	ubuf.resize(gb.usize);
	GzipBlockReader rd(fh, ubuf, b, gb.dict, gb.bits);
	rd.read();
}

std::string GzipFile::destName() const {
	using namespace PathUtils;
	std::string base = basename(path());
	if (replaceExtension(base, "tgz", "tar")) return base;
	if (removeExtension(base, "gz")) return base;
	return base;
}

namespace {
	enum {
		BlockBitsMask = 0x07,
		BlockDictFlag = 0x80,
	};
};

bool GzipFile::readBlock(FileHandle& fh, Block *b) {
	if (!IndexedCompFile::readBlock(fh, b))
		return false;
	
	GzipBlock *gb = dynamic_cast<GzipBlock*>(b);
	uint8_t flags;
	fh.readBE(flags);
	gb->bits = flags & BlockBitsMask;
	if (flags & BlockDictFlag)
		fh.read(gb->dict, WindowSize);
	
	return true;
}

void GzipFile::writeBlock(FileHandle& fh, const Block* b) const {
	IndexedCompFile::writeBlock(fh, b);
	
	const GzipBlock* gb = dynamic_cast<const GzipBlock*>(b);
	uint8_t flags = (gb->bits & BlockBitsMask);
	if (!gb->dict.empty())
		flags |= BlockDictFlag;
	
	fh.writeBE(flags);
	if (flags & BlockDictFlag)
		fh.write(gb->dict);
}
