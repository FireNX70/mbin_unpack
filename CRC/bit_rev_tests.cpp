#include <cstdint>
#include <iostream>

#include "CRC.hpp"

int main()
{
	constexpr uint8_t TEST_U8 = 0b10101010;
	constexpr uint8_t EX_U8 = 0b01010101;

	constexpr uint32_t TEST_U32 = 0b01101001010110100111010101100101;
	constexpr uint32_t EX_U32 = 0b10100110101011100101101010010110;

	uint8_t res;
	uint32_t res_32;

	res = bit_reverse(TEST_U8);
	if(res != EX_U8)
	{
		std::cout << "Value mismatch!!!" << std::endl;
		std::cout << std::hex << "Expected: " << (uint16_t)EX_U8 << std::endl;
		std::cout << "Got: " << (uint16_t)res << std::dec << std::endl;
		return 1;
	}

	res_32 = bit_reverse(TEST_U32);
	if(res_32 != EX_U32)
	{
		std::cout << "Value mismatch!!!" << std::endl;
		std::cout << std::hex << "Expected: " << EX_U32 << std::endl;
		std::cout << "Got: " << res_32 << std::dec << std::endl;
		return 2;
	}

	return 0;
};
