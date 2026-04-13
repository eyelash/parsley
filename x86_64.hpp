#pragma once

#include <cstdint>
#include <vector>

namespace x86_64 {

enum Register32: std::uint8_t {
	EAX,
	ECX,
	EDX,
	EBX,
	ESP,
	EBP,
	ESI,
	EDI,
	R8D,
	R9D,
	R10D,
	R11D,
	R12D,
	R13D,
	R14D,
	R15D,
	EIZ = ESP
};

enum Register64: std::uint8_t {
	RAX,
	RCX,
	RDX,
	RBX,
	RSP,
	RBP,
	RSI,
	RDI,
	R8,
	R9,
	R10,
	R11,
	R12,
	R13,
	R14,
	R15,
	RIZ = RSP
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

constexpr Address ADDR(Register64 base, Register64 index, Scale scale = S1, std::int32_t displacement = 0) {
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
	void RM(std::uint8_t op1, Register32 op2) {
		ModRM(3, op1 & 7, op2 & 7);
	}
	void RM(std::uint8_t op1, Register64 op2) {
		ModRM(3, op1 & 7, op2 & 7);
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
	void MR(Address op1, std::uint8_t op2) {
		RM(op2, op1);
	}
	void REX(std::uint8_t w, std::uint8_t r, std::uint8_t x, std::uint8_t b) {
		write<std::uint8_t>(0x40 | w << 3 | r << 2 | x << 1 | b);
	}
public:
	std::size_t get_position() const {
		return data.size();
	}
	const std::vector<char>& get_data() const {
		return data;
	}
	void MOV(Register64 dst, Register64 src) {
		REX(1, dst >> 3, 0, src >> 3);
		opcode(0x8B);
		RM(dst, src);
	}
	void MOV(Register64 dst, std::uint64_t imm) {
		REX(1, 0, 0, dst >> 3);
		opcode(0xB8 | dst & 7);
		write<std::uint64_t>(imm);
	}
	void SYSCALL() {
		opcode_0F(0x05);
	}
};

}
