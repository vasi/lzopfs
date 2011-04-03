#ifndef BITBUFFER_H
#define BITBUFFER_H

#include "lzopfs.h"

class BitBuffer {
protected:
	Buffer& mBuf;
	Buffer::const_iterator mRPos;
	size_t mRBits; // How many bits into read pos are we?
	size_t mWBits;
		
	// Ensure only lower b bits of v are set
	static uint8_t clip(size_t b, uint8_t v) {
		return uint8_t(v << (8 - b)) >> (8 - b);
	}
	
	// How many bits left to read/write in read/write pos?
	size_t rleft() const { return 8 - mRBits; }
	size_t wleft() const { return 8 - mWBits; }
	
	// Get some bits from only the read pos, not later
	uint8_t getLeft(size_t b = 0) {
		if (b == 0)
			b = rleft();
		else if (b > rleft())
			throw std::runtime_error("invalid bits for getLeft");
		
		if (mRPos == mBuf.end())
			throw std::runtime_error("read past end");
		uint8_t r = *mRPos;
		r = clip(rleft(), r) >> (rleft() - b);
		drop(b);
		return r;
	}
	
	void putLeft(size_t b, uint8_t v) {
		if (b > wleft() || b == 0)
			throw std::runtime_error("invalid bits for putLeft");
		if (mWBits == 0)
			mBuf.push_back(0); // need more room
		
		mBuf.back() |= clip(b, v) << (wleft() - b);
		mWBits = (mWBits + b) % 8;
	}
	
	void putByte(uint8_t b) {
		if (mWBits != 0)
			throw std::runtime_error("putByte when not byte-aligned");
		mBuf.push_back(b);
	}
		
public:
	BitBuffer(Buffer& buf, size_t bitpos = 0)
		: mBuf(buf), mRPos(buf.begin()), mRBits(bitpos), mWBits(0)
		{ drop(0); }
	
	void drop(size_t b) {
		mRBits += b;
		mRPos += mRBits / 8;
		mRBits %= 8;
		
		if (mRPos > mBuf.end() || (mRPos == mBuf.end() && mRBits > 0))
			throw std::runtime_error("read past end");
	}
	
	uint32_t get(size_t b) {
		uint32_t r = 0;
		while (b) {
			size_t want = std::min(b, rleft());
			r = (r << want) | getLeft(want);
			b -= want;
		}
		return r;
	}
	
	template <typename T>
	void put(size_t b, T v) {
		while (b) {
			size_t want = std::min(b, wleft());
			putLeft(want, v >> (b - want));
			b -= want;
		}
	}
	
	void take(BitBuffer& o, size_t b) {
		size_t want;
		if (mWBits != 0) {
			want = (b < wleft() + 8) ? b : std::min(b, wleft());
			put(want, o.get(want));
			b -= want;
		}
		
		for (; b >= 8; b -= 8)
			putByte(o.get(8));
		
		if (b)
			put(b, o.get(b));
	}
};

#endif // BITBUFFER_H
