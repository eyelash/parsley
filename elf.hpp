#pragma once

#include "common.hpp"
#include <cstdint>
#include <vector>
#include <fstream>

namespace elf {

class StringTable {
	std::vector<char> data;
public:
	StringTable() {
		data.push_back('\0');
	}
	const std::vector<char>& get_data() const {
		return data;
	}
	std::size_t size() const {
		return data.size();
	}
	std::uint32_t add(const StringView& s) {
		if (s.empty()) {
			return 0;
		}
		const std::uint32_t index = data.size();
		data.insert(data.end(), s.begin(), s.end());
		data.push_back('\0');
		return index;
	}
};

class Symbol {
public:
	enum class Type: unsigned char {
		NOTYPE,
		OBJECT,
		FUNC
	};
	enum class Binding: unsigned char {
		LOCAL,
		GLOBAL,
		WEAK
	};
	static constexpr unsigned char get_info(Type type, Binding binding) {
		return static_cast<unsigned char>(type) | (static_cast<unsigned char>(binding) << 4);
	}
	std::uint32_t name;
	std::uint32_t position;
	std::uint32_t size;
	unsigned char info;
	constexpr Symbol(std::uint32_t name, std::uint32_t position, std::uint32_t size, Type type, Binding binding = Binding::LOCAL): name(name), position(position), size(size), info(get_info(type, binding)) {}
};

class SymbolTable {
public:
	StringTable string_table;
	std::vector<Symbol> symbols;
	std::size_t size() const {
		return symbols.size();
	}
	void add(const StringView& name, std::uint32_t position, Symbol::Type type, Symbol::Binding binding = Symbol::Binding::LOCAL) {
		symbols.emplace_back(string_table.add(name), position, 0, type, binding);
	}
	void start_symbol(const StringView& name, std::uint32_t position, Symbol::Type type, Symbol::Binding binding = Symbol::Binding::LOCAL) {
		symbols.emplace_back(string_table.add(name), position, 0, type, binding);
	}
	void end_symbol(std::uint32_t position) {
		Symbol& symbol = symbols.back();
		symbol.size = position - symbol.position;
	}
};

class Assembler {
	using Addr = std::uint32_t;
	static constexpr Addr VADDR = 0x10000;
	static constexpr std::uint16_t ELF_HEADER_SIZE = 52;
	static constexpr std::uint16_t PROGRAM_HEADER_SIZE = 32;
	static constexpr std::uint16_t SECTION_HEADER_SIZE = 40;
	static constexpr Addr SYMBOL_SIZE = 16;
	// program header types
	static constexpr std::uint32_t PT_NULL = 0;
	static constexpr std::uint32_t PT_LOAD = 1;
	// program header flags
	static constexpr std::uint32_t PF_X = 1 << 0;
	static constexpr std::uint32_t PF_W = 1 << 1;
	static constexpr std::uint32_t PF_R = 1 << 2;
	// section header types
	static constexpr std::uint32_t SHT_NULL = 0;
	static constexpr std::uint32_t SHT_PROGBITS = 1;
	static constexpr std::uint32_t SHT_SYMTAB = 2;
	static constexpr std::uint32_t SHT_STRTAB = 3;
	// section header flags
	static constexpr std::uint32_t SHF_WRITE = 1 << 0;
	static constexpr std::uint32_t SHF_ALLOC = 1 << 1;
	static constexpr std::uint32_t SHF_EXECINSTR = 1 << 2;
	// symbol type
	static constexpr unsigned char STT_NOTYPE = 0;
	static constexpr unsigned char STT_OBJECT = 1;
	static constexpr unsigned char STT_FUNC = 2;
	// symbol binding
	static constexpr unsigned char STB_LOCAL = 0 << 4;
	static constexpr unsigned char STB_GLOBAL = 1 << 4;
	static constexpr unsigned char STB_WEAK = 2 << 4;
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
	void write(const std::vector<char>& data) {
		this->data.insert(this->data.end(), data.begin(), data.end());
	}
	void write(const StringTable& string_table) {
		write(string_table.get_data());
	}
public:
	const std::vector<char>& get_data() const {
		return data;
	}
	void write_elf_header(std::uint16_t program_header_count, std::uint16_t section_header_count, std::uint16_t string_table_index, Addr entry_point) {
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
		write<Addr>(entry_point); // entry point
		write<Addr>(ELF_HEADER_SIZE); // program header position
		write<Addr>(ELF_HEADER_SIZE + PROGRAM_HEADER_SIZE * program_header_count); // section header position
		write<std::uint32_t>(0); // flags
		write<std::uint16_t>(ELF_HEADER_SIZE); // ELF header size
		write<std::uint16_t>(PROGRAM_HEADER_SIZE); // program header size
		write<std::uint16_t>(program_header_count); // number of program headers
		write<std::uint16_t>(SECTION_HEADER_SIZE); // section header size
		write<std::uint16_t>(section_header_count); // number of section headers
		write<std::uint16_t>(string_table_index); // section header index for the string table
	}
	void write_program_header(Addr offset, std::uint32_t size) {
		write<std::uint32_t>(PT_LOAD); // type
		write<Addr>(offset); // offset
		write<Addr>(VADDR + offset); // virtual address
		write<Addr>(0); // physical address
		write<std::uint32_t>(size); // file size
		write<std::uint32_t>(size); // memory size
		write<std::uint32_t>(PF_R | PF_X); // flags
		write<std::uint32_t>(0); // alignment
	}
	void write_section_header(std::uint32_t name, std::uint32_t type, std::uint32_t flags, Addr offset, std::uint32_t size) {
		write<std::uint32_t>(name); // name
		write<std::uint32_t>(type); // type
		write<std::uint32_t>(flags); // flags
		write<Addr>(flags & SHF_ALLOC ? VADDR + offset : 0); // address
		write<Addr>(offset); // offset
		write<std::uint32_t>(size); // size
		write<std::uint32_t>(0); // link
		write<std::uint32_t>(0); // info
		write<std::uint32_t>(0); // address alignment
		write<std::uint32_t>(0); // entry size
	}
	void write_section_header_symtab(std::uint32_t name, Addr offset, std::uint32_t symbol_count, std::uint32_t string_table_index) {
		write<std::uint32_t>(name); // name
		write<std::uint32_t>(SHT_SYMTAB); // type
		write<std::uint32_t>(0); // flags
		write<Addr>(0); // address
		write<Addr>(offset); // offset
		write<std::uint32_t>(SYMBOL_SIZE * symbol_count); // size
		write<std::uint32_t>(string_table_index); // link
		write<std::uint32_t>(symbol_count); // info
		write<std::uint32_t>(0); // address alignment
		write<std::uint32_t>(SYMBOL_SIZE); // entry size
	}
	void write_symbol(std::uint32_t name, Addr value, std::uint32_t size, unsigned char info, std::uint16_t section_index) {
		write<std::uint32_t>(name); // name
		write<Addr>(value); // value
		write<std::uint32_t>(size); // size
		write<unsigned char>(info); // info
		write<unsigned char>(0); // other
		write<std::uint16_t>(section_index); // section header index
	}
	void assemble(const std::vector<char>& program, const SymbolTable& symbols) {
		StringTable string_table;
		constexpr std::uint16_t program_header_count = 1;
		constexpr std::uint16_t section_header_count = 5;
		constexpr Addr program_offset = ELF_HEADER_SIZE + PROGRAM_HEADER_SIZE * program_header_count + SECTION_HEADER_SIZE * section_header_count;
		constexpr Addr entry_point = VADDR + program_offset;
		write_elf_header(program_header_count, section_header_count, 4, entry_point);
		write_program_header(program_offset, program.size());
		{
			Addr offset = program_offset;
			write_section_header(0, SHT_NULL, 0, 0, 0);
			write_section_header(string_table.add(".text"), SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR, offset, program.size());
			offset += program.size();
			write_section_header_symtab(string_table.add(".symtab"), offset, 1 + symbols.size(), 3);
			offset += SYMBOL_SIZE * (1 + symbols.size());
			write_section_header(string_table.add(".strtab"), SHT_STRTAB, 0, offset, symbols.string_table.size());
			offset += symbols.string_table.size();
			auto shstrtab = string_table.add(".shstrtab");
			write_section_header(shstrtab, SHT_STRTAB, 0, offset, string_table.size());
		}
		write(program);
		write_symbol(0, 0, 0, 0, 0);
		for (const Symbol& symbol: symbols.symbols) {
			write_symbol(symbol.name, entry_point + symbol.position, symbol.size, symbol.info, 1);
		}
		write(symbols.string_table);
		write(string_table);
	}
	void write_file(const char* path) {
		std::ofstream file(path);
		file.write(data.data(), data.size());
	}
};

}
