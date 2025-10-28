#pragma once

#include <cstdint>
#include <vector>
#include <fstream>

namespace elf {

class Assembler {
	using Addr = std::uint32_t;
	static constexpr Addr VADDR = 0x10000;
	static constexpr Addr ELF_HEADER_SIZE = 52;
	static constexpr Addr PROGRAM_HEADER_SIZE = 32;
	std::vector<char> data;
	template <class T> void write(T t) {
		for (std::size_t i = 0; i < sizeof(T); ++i) {
			data.push_back(t & 0xFF);
			t >>= 8;
		}
	}
	template <class T> void write(std::size_t position, T t) {
		auto iterator = data.begin() + position;
		for (std::size_t i = 0; i < sizeof(T); ++i) {
			*iterator++ = t & 0xFF;
			t >>= 8;
		}
	}
public:
	void write_elf_header() {
		write<std::uint8_t>(0x7F);
		write<std::uint8_t>('E');
		write<std::uint8_t>('L');
		write<std::uint8_t>('F');
		write<std::uint8_t>(1); // ELFCLASS32
		write<std::uint8_t>(1); // ELFDATA2LSB
		write<std::uint8_t>(1); // version
		write<std::uint8_t>(0); // OS ABI
		write<std::uint8_t>(0); // ABI version
		while (data.size() < 16) {
			write<std::uint8_t>(0);
		}

		write<std::uint16_t>(2); // ET_EXEC
		write<std::uint16_t>(3); // EM_386
		write<std::uint32_t>(1);
		write<Addr>(VADDR + ELF_HEADER_SIZE + PROGRAM_HEADER_SIZE); // entry point
		write<Addr>(ELF_HEADER_SIZE); // program header position
		write<Addr>(0); // section header position
		write<std::uint32_t>(0); // flags
		write<std::uint16_t>(ELF_HEADER_SIZE); // ELF header size
		write<std::uint16_t>(PROGRAM_HEADER_SIZE); // program header size
		write<std::uint16_t>(1); // number of program headers
		write<std::uint16_t>(0); // section header size
		write<std::uint16_t>(0); // number of section headers
		write<std::uint16_t>(0);
	}
	void write_program_header() {
		write<std::uint32_t>(1); // PT_LOAD
		write<Addr>(0); // offset
		write<Addr>(VADDR); // vaddr
		write<Addr>(0); // paddr
		write<std::uint32_t>(0); // filesz, will be set later
		write<std::uint32_t>(0); // memsz, will be set later
		write<std::uint32_t>(5); // flags: PF_R|PF_X
		write<std::uint32_t>(0); // align
	}
	void write_headers() {
		write_elf_header();
		write_program_header();
	}
	void write_program(const std::vector<char>& program) {
		data.insert(data.end(), program.begin(), program.end());
	}
	void write_file(const char* path) {
		write<std::uint32_t>(68, data.size()); // filesz
		write<std::uint32_t>(72, data.size()); // memsz
		std::ofstream file(path);
		file.write(data.data(), data.size());
	}
};

}
