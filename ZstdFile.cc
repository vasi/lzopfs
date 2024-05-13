#ifdef HAVE_ZSTD

#include "ZstdFile.h"

#include <zstd.h>

namespace {
  const uint32_t MagicSkippable = 0x184D2A5E;
  const uint32_t MagicSeekTable = 0x8F92EAB1;
  const size_t FooterSize = 9;
  const size_t TableOverhead = 8;
  const uint8_t ChecksumFlag = (1<<7);
}

ZstdFile::ZstdFile(const std::string& path, uint64_t maxBlock) 
    : BlockListCompFile(path) {
  initialize(maxBlock);
}

std::string ZstdFile::destName() const {
	using namespace PathUtils;
	std::string base = basename(path());
	if (removeExtension(base, "zst")) return base;
	return base;
}

ZstdFile::SeekTableInfo ZstdFile::findSeekTable(FileHandle& fh) const {
  uint64_t footerOffset = fh.seek(-FooterSize, SEEK_END);
  
  SeekTableInfo info;
  uint32_t magic;
  uint8_t flags;
  fh.readLE(info.count);
  fh.readLE(flags);
  fh.readLE(magic);
  if (magic != MagicSeekTable) {
    throw std::runtime_error("no seek table");
  }
  info.hasChecksums = (flags & ChecksumFlag);

  info.start = footerOffset - info.totalSize();;
  fh.seek(info.start - TableOverhead, SEEK_SET);

  uint32_t tableMagic;
  uint32_t frameSize;
  fh.readLE(tableMagic);
  fh.readLE(frameSize);
  if (tableMagic != MagicSkippable) {
    throw std::runtime_error("bad skippable table magic");
  }
  if (frameSize != info.totalSize() + FooterSize) {
    throw std::runtime_error("bad frame size");
  }

  return info;
}

void ZstdFile::checkFileType(FileHandle &fh) {
  // Just look for the magic
  uint32_t magic;
  fh.readLE(magic);
  if (magic != ZSTD_MAGICNUMBER) {
    throwFormat("magic mismatch");
  }

  findSeekTable(fh);
  // TODO: handle zstd multi-block files with no seek table
}

struct ZstdStream {
  ZSTD_DStream* dstream;

  ZstdStream() {
    dstream = ZSTD_createDStream();
  }

  ~ZstdStream() {
    ZSTD_freeDStream(dstream);
  }
};

void ZstdFile::decompressBlock(const FileHandle& fh, const Block& b,
		Buffer& ubuf) const {
  ZstdStream stream;

  ubuf.resize(b.usize);
  ZSTD_outBuffer output = { ubuf.data(), ubuf.size(), 0 };

  size_t chunkSize = ZSTD_DStreamInSize();
  uint64_t coff = b.coff;
  uint64_t cend = b.coff + b.csize;
  Buffer inbuf;

  while (coff < cend) {
    size_t toread = std::min(cend - coff, chunkSize);
    fh.tryPRead(coff, inbuf, toread);
    coff += inbuf.size();
    ZSTD_inBuffer input = { inbuf.data(), inbuf.size(), 0 };

    while (input.pos < input.size) {
      size_t ret = ZSTD_decompressStream(stream.dstream, &output, &input);
      if (ZSTD_isError(ret)) {
        throw std::runtime_error("zstd error: " + std::string(ZSTD_getErrorName(ret)));
      }
    }
  }
}

void ZstdFile::buildIndex(FileHandle& fh) {
  SeekTableInfo info = findSeekTable(fh);
  fh.seek(info.start, SEEK_SET);

  uint64_t uoff = 0, coff = 0;
  for (uint32_t i = 0; i < info.count; ++i) {
    uint32_t csize, usize, checksum;
    fh.readLE(csize);
    fh.readLE(usize);
    if (info.hasChecksums) {
      fh.readLE(checksum);
    }
    addBlock(new Block(usize, csize, uoff, coff));
    uoff += usize;
    coff += csize;
  }
}


#endif // HAVE_ZSTD
