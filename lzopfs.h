#ifndef LZOPFS_H
#define LZOPFS_H

#include <vector>
#include <stdint.h>

typedef std::vector<uint8_t> Buffer;

struct Block {
	uint32_t usize, csize;
	uint64_t coff, uoff;
	void *userdata;
	
	Block(uint32_t us = 0, uint32_t cs = 0,
			uint32_t co = 0, uint32_t uo = 0, void *u = 0)
		: usize(us), csize(cs), coff(co), uoff(uo), userdata(u) { }
};

#endif // LZOPFS_H
