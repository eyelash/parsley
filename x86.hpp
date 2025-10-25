#pragma once

#include <cstdint>
#include <vector>

namespace x86 {

enum Register: std::uint8_t {
	EAX,
	ECX,
	EDX,
	EBX,
	ESP,
	EBP,
	ESI,
	EDI,
	EIZ = ESP
};

enum Scale: std::uint8_t {
	S1,
	S2,
	S4,
	S8
};

class Address {
public:
	std::int8_t base;
	std::int8_t index;
	std::int8_t scale;
	std::int32_t displacement;
	constexpr Address(std::int8_t base, std::int8_t index, std::int8_t scale, std::int32_t displacement): base(base), index(index), scale(scale), displacement(displacement) {}
};

constexpr Address ADDR(Register base, Register index, Scale scale = S1, std::int32_t displacement = 0) {
	return Address(base, index, scale, displacement);
}

class Assembler {
	std::vector<char> data;
	template <class T> void write(T t) {
		for (std::size_t i = 0; i < sizeof(T); ++i) {
			data.push_back(t & 0xFF);
			t >>= 8;
		}
	}
	void opcode(std::uint8_t opcode) {
		write<std::uint8_t>(opcode);
	}
	void opcode_0F(std::uint8_t opcode) {
		write<std::uint8_t>(0x0F);
		write<std::uint8_t>(opcode);
	}
	void ModRM(std::uint8_t mod, std::uint8_t reg, std::uint8_t rm) {
		write<std::uint8_t>(mod << 6 | reg << 3 | rm);
	}
	void SIB(std::uint8_t scale, std::uint8_t index, std::uint8_t base) {
		write<std::uint8_t>(scale << 6 | index << 3 | base);
	}
	void RM(std::uint8_t op1, Register op2) {
		ModRM(3, op1, op2);
	}
	void RM(std::uint8_t op1, Address op2) {
		if (op2.index == EIZ && op2.base != ESP) {
			if (op2.base == -1) {
				ModRM(0, op1, EBP);
				write<std::uint32_t>(op2.displacement);
			}
			else {
				if (op2.displacement == 0 && op2.base != EBP) {
					ModRM(0, op1, op2.base);
				}
				else if (op2.displacement >= -128 && op2.displacement < 128) {
					ModRM(1, op1, op2.base);
					write<std::uint8_t>(op2.displacement);
				}
				else {
					ModRM(2, op1, op2.base);
					write<std::uint32_t>(op2.displacement);
				}
			}
		}
		else {
			if (op2.base == -1) {
				ModRM(0, op1, ESP);
				SIB(op2.scale, op2.index, EBP);
				write<std::uint32_t>(op2.displacement);
			}
			else {
				if (op2.displacement == 0 && op2.base != EBP) {
					ModRM(0, op1, ESP);
					SIB(op2.scale, op2.index, op2.base);
				}
				else if (op2.displacement >= -128 && op2.displacement < 128) {
					ModRM(1, op1, ESP);
					SIB(op2.scale, op2.index, op2.base);
					write<std::uint8_t>(op2.displacement);
				}
				else {
					ModRM(2, op1, ESP);
					SIB(op2.scale, op2.index, op2.base);
					write<std::uint32_t>(op2.displacement);
				}
			}
		}
	}
public:
	const std::vector<char>& get_data() const {
		return data;
	}
	void LEA(Register dst, Address src) {
		opcode(0x8D);
		RM(dst, src);
	}
};

}
