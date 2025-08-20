#ifndef CRC_TYPES_HEADER_INCLUDE_GUARD
#define CRC_TYPES_HEADER_INCLUDE_GUARD

#include <concepts>
#include <cstdint>

template <std::integral T>
struct CRC_consts_t
{
	T POLY;
	T INIT;
	T FINAL_XOR;
	bool BIT_REV_IN;
	bool BIT_REV_OUT;
	bool BYTESWAP_OUT;
};

#endif
