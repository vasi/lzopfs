#ifndef LZOPFS_H
#define LZOPFS_H

#include <vector>
#include <stdint.h>

typedef std::vector<uint8_t> Buffer;

struct Block {
	uint32_t usize, csize;
	uint64_t coff, uoff;
	
	Block(uint32_t us, uint32_t cs, uint32_t co, uint32_t uo)
		: usize(us), csize(cs), coff(co), uoff(uo) { }
};

#endif // LZOPFS_H
