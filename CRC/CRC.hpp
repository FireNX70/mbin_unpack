#ifndef CRC_HEADER_INCLUDE_GUARD
#define CRC_HEADER_INCLUDE_GUARD

#include <cstdint>
#include <concepts>
#include <bit>

#include "CRC_types.hpp"

template <std::integral T>
T bit_reverse(T n)
{
	T res;

	res = 0;
	for(uint8_t i = sizeof(T) * 8 - 1; i != 255; i--)
	{
		res |= (n & 1) << i; //May trigger UB on signed ints
		n >>= 1;
	}

	return res;
}

template <std::integral T>
T NOP(T n)
{
	return n;
};

/*Based on
 *https://barrgroup.com/blog/crc-series-part-3-crc-implementation-code-cc, by
 *Michael Barr.*/
template <typename T>
requires std::integral<T>
T basic_CRC(const void *data, const size_t size, const CRC_consts_t<T> &consts)
{
	constexpr uint8_t CRC_BIT_SIZE = sizeof(T) * 8;
	constexpr T MASK = 1 << (CRC_BIT_SIZE - 1);

	/*If we don't mind potentially generating a gazillion implementations for
	 *this, we could always move CRC_consts_t's flags to template args. That
	 *would remove three branches (not a big deal IMO) and would allow us to
	 *clean things up a bit. The most relevant one is BIT_REV_IN.*/

	uint8_t (* const prep_data_f)(uint8_t n) = consts.BIT_REV_IN ?
		bit_reverse<uint8_t> : NOP<uint8_t>;

	T CRC;

	CRC = consts.INIT;

	for(size_t i = 0; i < size; i++)
	{
		CRC ^= prep_data_f(((uint8_t*)data)[i]) << (CRC_BIT_SIZE - 8);

		for(uint8_t j = 0; j < 8; j++)
		{
			if(CRC & MASK) CRC = (CRC << 1) ^ consts.POLY;
			else CRC = (CRC << 1);
		}
	}

	/*I'm not really sure about the order of operations here. I'm fairly certain
	 *the byteswap should be the last one; but I'm not sure at all about the
	 *order of reverse and xor. The code this is based on reverses first, then
	 *xors; so that's what I'm doing.*/
	if(consts.BIT_REV_OUT) CRC = bit_reverse(CRC);
	CRC ^= consts.FINAL_XOR;
	if(consts.BYTESWAP_OUT) CRC = std::byteswap(CRC);

	return CRC;
}

#endif
