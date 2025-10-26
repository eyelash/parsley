#pragma once

#include <cstdint>
#include <vector>

namespace aarch64 {

enum Register: std::uint8_t {
	W0,
	W1,
	W2,
	W3,
	W4,
	W5,
	W6,
	W7,
	W8,
	W9,
	W10,
	W11,
	W12,
	W13,
	W14,
	W15,
	W16,
	W17,
	W18,
	W19,
	W20,
	W21,
	W22,
	W23,
	W24,
	W25,
	W26,
	W27,
	W28,
	W29,
	W30,
	WSP,
	WZR = WSP
};

enum Shift: std::uint8_t {
	LSL_0,
	LSL_16
};

class Assembler {
	std::vector<char> data;
	void instruction(std::uint32_t inst) {
		for (std::size_t i = 0; i < sizeof(std::uint32_t); ++i) {
			data.push_back(inst & 0xFF);
			inst >>= 8;
		}
	}
public:
	const std::vector<char>& get_data() const {
		return data;
	}
	void MOV(Register dst, Register src) {
		ORR(dst, WZR, src);
	}
	void MOVZ(Register dst, std::uint16_t imm, Shift shift = LSL_0) {
		instruction(0b0'10'100101'00'0000000000000000'00000 | shift << 21 | imm << 5 | dst);
	}
	void MOVK(Register dst, std::uint16_t imm, Shift shift = LSL_0) {
		instruction(0b0'11'100101'00'0000000000000000'00000 | shift << 21 | imm << 5 | dst);
	}
	void ORR(Register dst, Register lhs, Register rhs) {
		instruction(0b0'01'01010'00'0'00000'000000'00000'00000 | rhs << 16 | lhs << 5 | dst);
	}
	void AND(Register dst, Register lhs, Register rhs) {
		instruction(0b0'00'01010'00'0'00000'000000'00000'00000 | rhs << 16 | lhs << 5 | dst);
	}
};

}
