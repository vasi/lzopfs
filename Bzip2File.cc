#include "Bzip2File.h"

#include "PathUtils.h"

#include <algorithm>

#include <bzlib.h>

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

void Bzip2File::findBlockBoundaryCandidates(FileHandle& fh, BoundList &bl)
		const {
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
	
	const off_t fsz = fh.size();
	uint8_t buf[ChunkSize];
	fh.seek(0, SEEK_SET);
	size_t bsz = fh.tryRead(buf, ChunkSize);
	off_t rpos = bsz; // next read pos
	const size_t us = sizeof(uint64_t);
	
	const size_t ftsz = BlockMagicBytes + sizeof(uint32_t);
	const size_t lcreset = sizeof(Magic);
	size_t lcount = lcreset + 1; // how many more bytes til we can find a level?
	char level = 0;
	
	while (true) {
		const uint8_t *blast = buf + bsz - us;
		for (uint8_t *i = buf; i < blast; ++i) {
			if (lcount && !--lcount)
				level = *i;
			
			const int8_t b = backBits[*i];
			if (b == -1)
				continue;
			
			// Candidate byte, check if this is a magic
			uint64_t v = *reinterpret_cast<uint64_t*>(i);
			FileHandle::convertBE(v);
			v >>= 8 * (sizeof(uint64_t) - BlockMagicBytes) + b;
			const uint64_t u = *(i - 1);
			v |= (u << ((8 * BlockMagicBytes) - b)) & BlockMagicMask;
			
			const off_t pos = rpos - (buf + bsz - i);
			if (v == BlockMagic || v == EOSMagic) {
				DEBUG("%s %c %9lld %hhu",
					(v == BlockMagic ? "block" : "eos  "), level, pos, b);
				bl.push_back(BlockBoundary(v, level, pos, b));
				if (v == EOSMagic)
					lcount = ftsz + lcreset;
			}
		}
		if (fsz == rpos)
			return;
		std::copy(buf + bsz - us - 1, buf + bsz, buf);
		const size_t r = fh.tryRead(buf + us + 1, ChunkSize - us - 1);
		bsz = r + us + 1;
		rpos += r;
	}
}

// Create a buffer that looks like a one-block file
void Bzip2File::createAlignedBlock(const FileHandle& fh, Buffer& b,
		char level, off_t coff, size_t bits, off_t end, size_t endbits)
		const {
	// Magic + level
	const size_t prev = (bits ? 1 : 0);
	const size_t sbits = 8 - bits;
	b.resize(sizeof(Magic) + sizeof(uint8_t) + prev + (end - coff)
		+ BlockMagicBytes + sizeof(uint32_t));
	uint8_t *ip = &b[0];
	std::copy(Magic, Magic + sizeof(Magic), ip);
	ip += sizeof(Magic);
	*ip++ = level;
	
	// Data
	const uint8_t *fp = ip + fh.tryPRead(coff - prev, ip, end - coff + prev);
	if (bits) {
		for (; ip < fp; ++ip)
			*ip = (*ip << sbits) | (*(ip + 1) >> bits);
	}
	
	// How many bits should be empty at the end?
	const size_t ebitsp = 8 * prev - bits + endbits;
	const size_t ebits = ebitsp % 8;
	const size_t sebits = 8 - ebits;
	ip += (fp - ip) - ebitsp / 8;
	if (ebitsp >= 8)
		*ip = 0;
	
	// Setup footer: EOS + CRC 
	uint8_t fbuf[sizeof(uint64_t) + sizeof(uint32_t)];
	uint64_t *eos = reinterpret_cast<uint64_t*>(fbuf);
	*eos = EOSMagic;
	FileHandle::convertBE(*eos);
	uint32_t *crc = reinterpret_cast<uint32_t*>(fbuf + sizeof(uint64_t));
	const uint32_t* const icrc = reinterpret_cast<const uint32_t*>(
		&b[sizeof(Magic) + sizeof(uint8_t) + BlockMagicBytes]);
	*crc = *icrc;
	
	// Write footer
	const uint8_t *ep = fbuf + sizeof(uint64_t) - BlockMagicBytes;
	if (ebits)
		*(ip - 1) = ((*(ip - 1) >> ebits) << ebits) | (*ep >> sebits);
	for (; ep < fbuf + sizeof(fbuf) - 1; ++ep)
		*ip++ = (*ep << ebits) | (*(ep + 1) >> sebits);
	*ip++ = *ep << ebits;
}
	
void Bzip2File::decompress(const Buffer& in, Buffer& out) const {
	bz_stream s;
	s.bzalloc = NULL;
	s.bzfree = NULL;
	s.opaque = NULL;
	int err = BZ2_bzDecompressInit(&s, 0, 0);
	if (err != BZ_OK)
		throw std::runtime_error("bzip2 init");
	
	s.next_in = const_cast<char*>(reinterpret_cast<const char*>(&in[0]));
	s.avail_in = in.size();
	if (!out.empty())
		s.next_out = reinterpret_cast<char*>(&out[0]);
	s.avail_out = out.size();
	
	while (true) {
		if (s.avail_out == 0) {
			out.resize(out.size() + ChunkSize);
			s.next_out = reinterpret_cast<char*>(
				&out[out.size() - ChunkSize]);
			s.avail_out = ChunkSize;
		}
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
	out.resize(out.size() - s.avail_out);
}

void Bzip2File::buildIndex(FileHandle& fh) {
	BoundList bl;
	findBlockBoundaryCandidates(fh, bl);
	
	// Build blocklist from boundaries
	off_t uoff = 0;
	Buffer in, out;
	BoundList::iterator i = bl.begin(), j = bl.begin();
	char level = i->level;
	for (++j; j != bl.end(); ) {
		if (i->magic != EOSMagic) {
			if (level == 0)
				level = i->level;
			//DEBUG("level = %c", level);
			createAlignedBlock(fh, in, level, i->coff, i->bits, j->coff,
				j->bits);			
			try {
				decompress(in, out);
			} catch (std::runtime_error& e) { // Boundary spurious, remove it
				DEBUG("failed! %9lld -- %9lld", i->coff, j->coff);
				//FileHandle::writeBuf(in, "block.bz2"); exit(-1);
				bl.erase(j++);
				continue;
			}
			DEBUG("ok! %9lld -- %9lld", i->coff, j->coff);
			addBlock(new Bzip2Block(*i, *j, uoff, out.size(), level));
			uoff += out.size();
			if (j->magic == EOSMagic)
				level = 0;
		}
		++i; ++j;
	}
}

void Bzip2File::decompressBlock(const FileHandle& fh, const Block& b,
		Buffer& ubuf) const {
	Buffer in;
	const Bzip2Block& bb = dynamic_cast<const Bzip2Block&>(b);
	createAlignedBlock(fh, in, bb.level, bb.coff, bb.bits,
		bb.coff + bb.csize, bb.endbits);
	decompress(in, ubuf);
	if (ubuf.size() != bb.usize)
		throw std::runtime_error("bzip2 block decompresses to wrong size");
}

std::string Bzip2File::destName() const {
	using namespace PathUtils;
	std::string base = basename(path());
	if (replaceExtension(base, "tbz2", "tar")) return base;
	if (removeExtension(base, "bz2")) return base;
	return base;
}

bool Bzip2File::readBlock(FileHandle& fh, Block *b) {
	if (!IndexedCompFile::readBlock(fh, b))
		return false;
	
	Bzip2Block *bb = dynamic_cast<Bzip2Block*>(b);
	uint8_t bits;
	fh.readBE(bits);
	bb->bits = bits & 0xF;
	bb->endbits = bits >> 4;
	fh.readBE(bb->level);
	
	return true;
}

void Bzip2File::writeBlock(FileHandle& fh, const Block* b) const {
	IndexedCompFile::writeBlock(fh, b);
	
	const Bzip2Block* bb = dynamic_cast<const Bzip2Block*>(b);
	uint8_t bits = (bb->endbits << 4) + bb->bits;
	fh.writeBE(bits);
	fh.writeBE(bb->level);
}
