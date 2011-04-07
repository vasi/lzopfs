#include "Bzip2File.h"

#include "BitBuffer.h"
#include "PathUtils.h"

#include <bzlib.h>

const char Bzip2File::Magic[3] = { 'B', 'Z', 'h' };

void Bzip2File::checkFileType(FileHandle& fh) {
	try {
		Buffer buf;
		fh.read(buf, sizeof(Magic));
		if (!std::equal(buf.begin(), buf.end(), Magic))
			throwFormat("magic");
		
		fh.readBE(mLevel);
		if (mLevel < '1' || mLevel > '9')
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
	/* Block boundaries are not byte aligned, but checking each bit is
	 * too expensive. So we only look at bytes that may be the first full
	 * byte of a block magic number.
	 *
	 * We look up each byte in the table to determine whether it could be
	 * the start of a magic number, and if so how many bits from the
	 * previous byte we require. No first-byte appears twice in the shifted
	 * magics, so a single table is fine. */
	int8_t backBits[256];
	for (size_t i = 0; i < 256; ++i)
		backBits[i] = -1;
	uint8_t m1 = BlockMagic >> ((BlockMagicBytes - 1) * 8),
		m2 = (BlockMagic >> ((BlockMagicBytes - 2) * 8)) & 0xff,
		e1 = EOSMagic >> ((BlockMagicBytes - 1) * 8),
		e2 = (EOSMagic >> ((BlockMagicBytes - 2) * 8)) & 0xff;		
	for (size_t i = 0; i < 8; ++i) {
		uint8_t m = (m1 << i) | (m2 >> (8 - i)),
			e = (e1 << i) | (e2 >> (8 - i));
		backBits[m] = backBits[e] = i;
	}
	
	uint8_t buf[ChunkSize];
	fh.seek(0, SEEK_SET);
	size_t bsz = fh.tryRead(buf, ChunkSize);
	off_t rpos = bsz;
	const size_t us = sizeof(uint64_t);
	while (true) {
		const uint8_t *blast = buf + bsz - us;
		for (uint8_t *i = buf; i < blast; ++i) {
			const int8_t b = backBits[*i];
			if (b == -1)
				continue;
			
			// Candidate byte, check if this is a magic
			uint64_t v = *reinterpret_cast<uint64_t*>(i);
			FileHandle::convertBE(v);
			v >>= 16 + b;
			const uint64_t u = *(i - 1);
			v |= (u << (48 - b)) & BlockMagicMask;
			
			const off_t pos = rpos - (buf + bsz - i);
			if (v == BlockMagic) {
				addBlock(new Bzip2Block(pos, b));
				//printf("magic %9lld\n", pos);
			} else if (v == EOSMagic) {
				mEOS = pos;
				mEOSBits = b;
				//printf("end %9lld\n", pos);
				return;
			}
		}
		std::copy(buf + bsz - us - 1, buf + bsz, buf);
		const size_t r = fh.tryRead(buf + us + 1, ChunkSize - us - 1);
		bsz = r + us + 1;
		rpos += r;
	}
}

bool Bzip2File::tryDecompress(FileHandle& fh, off_t coff, size_t bits,
		off_t end, size_t endbits) {
	// Create a buffer that looks like a one-block file
	Buffer in;
	BitBuffer ibuf(in);
	
	for (size_t i = 0; i < sizeof(Magic); ++i)
		ibuf.put(8, Magic[i]);					// magic
	ibuf.put(8, mLevel);						// level
	
	Buffer read;
	fh.seek(coff - 1, SEEK_SET);
	fh.read(read, end - fh.tell());
	BitBuffer rbuf(read, 8 - bits);
	ibuf.take(rbuf, 8 * (end - coff) + bits - endbits);	// data
	
	uint64_t eos = EOSMagic;
	FileHandle::convertBE(eos);
	uint8_t *eosp = reinterpret_cast<uint8_t*>(&eos) + sizeof(uint64_t)
		- BlockMagicBytes;
	for (size_t i = 0; i < BlockMagicBytes; ++i)
		ibuf.put(8, eosp[i]);					// eos
	
	size_t crc = sizeof(Magic) + 1 + BlockMagicBytes;
	for (size_t i = crc; i < crc + sizeof(uint32_t); ++i)
		ibuf.put(8, in[i]);						// crc32
	
	bz_stream s;
	s.bzalloc = NULL;
	s.bzfree = NULL;
	s.opaque = NULL;
	int err = BZ2_bzDecompressInit(&s, 0, 0);
	if (err != BZ_OK)
		throw std::runtime_error("bzip2 init");
	
	s.next_in = reinterpret_cast<char*>(&in[0]);
	s.avail_in = in.size();
	Buffer out(CompressedFile::ChunkSize);
	while (true) {
		s.next_out = reinterpret_cast<char*>(&out[0]);
		s.avail_out = out.size();
		err = BZ2_bzDecompress(&s);
		if (err == BZ_STREAM_END)
			break;
		if (err != BZ_OK)
			throw std::runtime_error("bzip2 decompress");
		if (s.avail_in == 0 && s.avail_out == out.size())
			throw std::runtime_error("bzip2 no progress");
	}
	
	err = BZ2_bzDecompressEnd(&s);
	if (err != BZ_OK)
		throw std::runtime_error("bzip2 end");
		
	return true;
	// fixme: return false?
}

void Bzip2File::buildIndex(FileHandle& fh) {
	findBlockBoundaryCandidates(fh);
	dumpBlocks();
	
	for (BlockList::iterator i = mBlocks.begin(); i != mBlocks.end(); ++i) {		
		Bzip2Block* bb = dynamic_cast<Bzip2Block*>(*i);
		off_t end;
		size_t endbits;
		if (i + 1 == mBlocks.end()) {
			end = mEOS;
			endbits = mEOSBits;
		} else {
			Bzip2Block* nb = dynamic_cast<Bzip2Block*>(*(i + 1));
			end = nb->coff;
			endbits = nb->bits;
		}
		try {
			tryDecompress(fh, bb->coff, bb->bits, end, endbits);
			fprintf(stderr, "decomp %9lld ok!\n", bb->coff);
		} catch (std::runtime_error& e) {
			fprintf(stderr, "error decomp %9lld: %s\n", bb->coff, e.what());
		}
	}
	
	exit(-1);
}

void Bzip2File::decompressBlock(FileHandle& fh, const Block& b,
		Buffer& ubuf) {
}

std::string Bzip2File::destName() const {
	using namespace PathUtils;
	std::string base = basename(path());
	if (replaceExtension(base, "tbz2", "tar")) return base;
	if (removeExtension(base, "bz2")) return base;
	return base;
}
