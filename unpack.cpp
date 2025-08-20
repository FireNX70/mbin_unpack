#include <cstdint>
#include <string>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <memory>
#include <cstring>
#include <bit>
#include <iterator>

#include "CRC/CRC.hpp"

namespace On_disk_sizes
{
	constexpr uint8_t HEADER = 128;
}

namespace On_disk_addrs
{
	constexpr uint8_t FTYPE = 0x41;
	constexpr uint8_t DATA_SIZE = 0x53;
	constexpr uint8_t RES_SIZE = 0x57;
	constexpr uint8_t COMM_LENGTH = 0x63;
	constexpr uint8_t SEC_HDR_SIZE = 0x78;
	constexpr uint8_t MBINII_MIN_VER = 0x7B;
	constexpr uint8_t CRC = 124;
}

struct header_t //Keep only the relevant parts
{
	std::string fname;
	char ftype[5];
	uint32_t data_size;
	uint32_t res_size;
	uint16_t comm_len; //Should also be size
	uint16_t sec_hdr_size;
};

enum struct ERR_e: uint8_t
{
	NX_FILE = 1,
	INV_FILE,
	TOO_SMALL,
	NOT_MBIN,
	MBIN_TOO_NEW,
	BAD_CRC,
	IO_ERROR
};

constexpr CRC_consts_t<uint16_t> XMODEM_CONSTS =
{
	.POLY = 0x1021,
	.INIT = 0,
	.FINAL_XOR = 0,
	.BIT_REV_IN = false,
	.BIT_REV_OUT = false,
	.BYTESWAP_OUT = false,
};

constexpr uint8_t MAC_BIN_VER = 0x81;

uint8_t load_header(std::ifstream &src, header_t &dst)
{
	uint16_t CRC, file_CRC;

	std::unique_ptr<uint8_t[]> data =
		std::make_unique<uint8_t[]>(On_disk_sizes::HEADER);

	src.read((char*)data.get(), On_disk_sizes::HEADER);

	/*We care only about the first two bytes and the CRC check, because the
	 *magic was apparently added in MacBinary III (I can confirm MacBinary III
	 *writes it, haven't checked MacBinary II). We do check that the old version
	 *number (the first byte) is 0.*/

	//Old ver
	if(data[0]) return (uint8_t)ERR_e::NOT_MBIN;

	//fname len
	if(!data[1] || data[1] > 63) return (uint8_t)ERR_e::NOT_MBIN;

	if(data[On_disk_addrs::MBINII_MIN_VER] > MAC_BIN_VER)
		return (uint8_t)ERR_e::MBIN_TOO_NEW;

	CRC = basic_CRC<uint16_t>(data.get(), On_disk_addrs::CRC, XMODEM_CONSTS);
	std::memcpy(&file_CRC, &data[On_disk_addrs::CRC], 2);

	if constexpr(std::endian::native != std::endian::big)
		file_CRC = std::byteswap(file_CRC);

	if(CRC != file_CRC) return (uint8_t)ERR_e::BAD_CRC;

	dst.fname.resize(data[1]);
	std::memcpy(dst.fname.data(), data.get() + 2, data[1]);

	std::memcpy(dst.ftype, &data[On_disk_addrs::FTYPE], 4);
	dst.ftype[4] = 0;

	std::memcpy(&dst.data_size, &data[On_disk_addrs::DATA_SIZE], 4);
	std::memcpy(&dst.res_size, &data[On_disk_addrs::RES_SIZE], 4);
	std::memcpy(&dst.comm_len, &data[On_disk_addrs::COMM_LENGTH], 2);
	std::memcpy(&dst.sec_hdr_size, &data[On_disk_addrs::SEC_HDR_SIZE], 2);

	if constexpr(std::endian::native != std::endian::big)
	{
		dst.data_size = std::byteswap(dst.data_size);
		dst.res_size = std::byteswap(dst.res_size);
		dst.comm_len = std::byteswap(dst.comm_len);
		dst.sec_hdr_size = std::byteswap(dst.sec_hdr_size);
	}

	return 0;
}

uint8_t unpack(const std::filesystem::path &src)
{
	const uintmax_t fsize = std::filesystem::file_size(src);

	uint8_t status, rem;

	std::ifstream ifstr(src, std::ios_base::binary);
	std::ofstream ofstr;
	std::istreambuf_iterator<std::ifstream::char_type> ifs_it;
	std::ostreambuf_iterator<std::ofstream::char_type> ofs_it = ofstr;

	header_t header;

	if(fsize < On_disk_sizes::HEADER) return (uint8_t)ERR_e::TOO_SMALL;

	status = load_header(ifstr, header);
	if(status) return status;

	const std::filesystem::path BASE_DST_PATH =
		(src.parent_path() / header.fname)
			.concat(std::filesystem::path(header.fname).has_extension() ? ""
				: std::string(".") + header.ftype);

	/*The secondary header (if present) is supposed to follow the header. The
	 *comment is supposed to follow the resource fork, according to the
	 *MacBinary II spec. The MacBinary II+ spec contradicts itself by first
	 *stating the comment should follow the resource fork, then stating it
	 *should follow immediately after the secondary header (so before even the
	 *data fork). Given that it was never widely adopted, and its
	 *self-contradictory nature, I'm just going to completely ignore it.*/

	rem = header.sec_hdr_size ? 128 - (header.sec_hdr_size % 128) : 0;
	const uint32_t DATA_BASE = On_disk_sizes::HEADER + header.sec_hdr_size +
		rem;

	if(header.data_size)
	{
		if(fsize < DATA_BASE + header.data_size)
			return (uint8_t)ERR_e::TOO_SMALL;

		ofstr.open(BASE_DST_PATH, std::ios_base::binary);
		if(!ofstr.is_open() || !ofstr.good()) return (uint8_t)ERR_e::IO_ERROR;

		ifstr.seekg(DATA_BASE);
		ifs_it = ifstr;
		ofs_it = ofstr;

		for(uint32_t i = 0; i < header.data_size; i++) *ofs_it++ = *ifs_it++;

		if(ofs_it.failed()) return (uint8_t)ERR_e::IO_ERROR;
		ofstr.close();
	}

	rem = header.data_size ? 128 - (header.data_size % 128) : 0;
	const uintmax_t RES_ADDR = DATA_BASE + header.data_size + rem;

	if(header.res_size)
	{
		if(fsize < RES_ADDR + header.res_size) return (uint8_t)ERR_e::TOO_SMALL;

		ofstr.open(BASE_DST_PATH.string() + ".res", std::ios_base::binary);
		if(!ofstr.is_open() || !ofstr.good()) return (uint8_t)ERR_e::IO_ERROR;

		ifstr.seekg(RES_ADDR);
		ifs_it = ifstr;
		ofs_it = ofstr;

		for(uint32_t i = 0; i < header.res_size; i++) *ofs_it++ = *ifs_it++;

		if(ofs_it.failed()) return (uint8_t)ERR_e::IO_ERROR;
	}

	rem = header.res_size ? 128 - (header.res_size % 128) : 0;
	const uintmax_t COM_BASE = RES_ADDR + header.res_size + rem;

	if(header.comm_len)
	{
		if(fsize < COM_BASE + header.comm_len) return (uint8_t)ERR_e::TOO_SMALL;

		ofstr.open(BASE_DST_PATH.string() + ".cmt", std::ios_base::binary);
		if(!ofstr.is_open() || !ofstr.good()) return (uint8_t)ERR_e::IO_ERROR;

		ifstr.seekg(COM_BASE);
		ifs_it = ifstr;
		ofs_it = ofstr;

		for(uint32_t i = 0; i < header.comm_len; i++) *ofs_it++ = *ifs_it++;

		if(ofs_it.failed()) return (uint8_t)ERR_e::IO_ERROR;
	}

	return 0;
}

void print_err(const uint8_t err)
{
	switch((ERR_e)err)
	{
		using enum ERR_e;

		case NX_FILE:
			std::cerr << "File does not exist!!!" << std::endl;
			break;

		case INV_FILE:
			std::cerr << "Invalid file!!!" << std::endl;
			break;

		case TOO_SMALL:
			std::cerr << "File too small!!!" << std::endl;
			break;

		case NOT_MBIN:
			std::cerr << "Not a MacBinary file!!!" << std::endl;
			break;

		case MBIN_TOO_NEW:
			std::cerr << "MacBinary minimum version is too new!!!";
			std::cerr << " Max supported is " << (uint16_t)MAC_BIN_VER << std::endl;
			break;

		case BAD_CRC:
			std::cerr << "CRC mismatch!!!" << std::endl;
			break;

		case IO_ERROR:
			std::cerr << "IO error!!!" << std::endl;
			break;

		default:
			std::cerr << "Unknown error!!!" << std::endl;
			break;
	}

	std::cerr << std::endl;
};

int main(int argc, char **argv)
{
	if(argc < 2)
	{
		std::cerr << "Need at least one path as source!!!" << std::endl;
		std::cerr << std::endl;
		return (uint8_t)ERR_e::NX_FILE;
	}

	for(int i = 1; i < argc; i++)
	{
		const std::filesystem::path fpth = argv[i];

		if(!std::filesystem::exists(fpth))
		{
			print_err((uint8_t)ERR_e::NX_FILE);
			return (uint8_t)ERR_e::NX_FILE;
		}

		if(!std::filesystem::is_regular_file(fpth))
		{
			print_err((uint8_t)ERR_e::INV_FILE);
			return (uint8_t)ERR_e::INV_FILE;
		}

		const uint8_t status = unpack(argv[i]);
		if(status)
		{
			print_err(status);
			return status;
		}
	}

	std::cout << "Unpack OK!" << std::endl;
	std::cout << std::endl;

	return 0;
}
