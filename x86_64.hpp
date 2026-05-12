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

class ScaledIndex {
public:
	std::int8_t index;
	std::int8_t scale;
	constexpr ScaledIndex(std::int8_t index, std::int8_t scale): index(index), scale(scale) {}
};

class AddressComputation {
public:
	std::int8_t base;
	std::int8_t index;
	std::int8_t scale;
	std::int32_t displacement;
	constexpr AddressComputation(std::int8_t base, std::int8_t index, std::int8_t scale, std::int32_t displacement): base(base), index(index), scale(scale), displacement(displacement) {}
	constexpr AddressComputation(Register64 base): AddressComputation(base, RIZ, S1, 0) {}
	constexpr AddressComputation(ScaledIndex scaled_index): AddressComputation(-1, scaled_index.index, scaled_index.scale, 0) {}
	constexpr AddressComputation(std::int32_t displacement): AddressComputation(-1, RIZ, S1, displacement) {}
	constexpr AddressComputation operator +(std::int32_t displacement) const {
		return AddressComputation(base, index, scale, this->displacement + displacement);
	}
};

constexpr ScaledIndex operator *(Register64 index, Scale scale) {
	return ScaledIndex(index, scale);
}
constexpr AddressComputation operator +(Register64 base, ScaledIndex scaled_index) {
	return AddressComputation(base, scaled_index.index, scaled_index.scale, 0);
}
constexpr AddressComputation operator +(Register64 base, Register64 index) {
	return AddressComputation(base, index, S1, 0);
}
constexpr AddressComputation operator +(Register64 base, std::int32_t displacement) {
	return AddressComputation(base, RIZ, S1, displacement);
}
constexpr AddressComputation operator +(ScaledIndex scaled_index, std::int32_t displacement) {
	return AddressComputation(-1, scaled_index.index, scaled_index.scale, displacement);
}

constexpr Address ADDR(Register64 base, Register64 index, Scale scale = S1, std::int32_t displacement = 0) {
	return Address(base, index, scale, displacement);
}
constexpr struct {
	constexpr Address operator [](const AddressComputation& address_computation) const {
		return Address(address_computation.base, address_computation.index, address_computation.scale, address_computation.displacement);
	}
} PTR;

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
		if (op2.index == RIZ && (op2.base & 7) != RSP && op2.base != -1) {
			{
				if (op2.displacement == 0 && (op2.base & 7) != RBP) {
					ModRM(0, op1 & 7, op2.base & 7);
				}
				else if (op2.displacement >= -128 && op2.displacement < 128) {
					ModRM(1, op1 & 7, op2.base & 7);
					write<std::uint8_t>(op2.displacement);
				}
				else {
					ModRM(2, op1 & 7, op2.base & 7);
					write<std::uint32_t>(op2.displacement);
				}
			}
		}
		else {
			if (op2.base == -1) {
				ModRM(0, op1 & 7, RSP);
				SIB(op2.scale, op2.index & 7, RBP);
				write<std::uint32_t>(op2.displacement);
			}
			else {
				if (op2.displacement == 0 && (op2.base & 7) != RBP) {
					ModRM(0, op1 & 7, RSP);
					SIB(op2.scale, op2.index & 7, op2.base & 7);
				}
				else if (op2.displacement >= -128 && op2.displacement < 128) {
					ModRM(1, op1 & 7, RSP);
					SIB(op2.scale, op2.index & 7, op2.base & 7);
					write<std::uint8_t>(op2.displacement);
				}
				else {
					ModRM(2, op1 & 7, RSP);
					SIB(op2.scale, op2.index & 7, op2.base & 7);
					write<std::uint32_t>(op2.displacement);
				}
			}
		}
	}
	void MR(Address op1, std::uint8_t op2) {
		RM(op2, op1);
	}
	void REX(std::uint8_t w, std::uint8_t r, std::uint8_t x, std::uint8_t b) {
		if (w | r | x | b) {
			write<std::uint8_t>(0x40 | w << 3 | r << 2 | x << 1 | b);
		}
	}
	struct Opcode {
		std::uint8_t value;
		constexpr Opcode(std::uint8_t value): value(value) {}
	};
	struct Prefix {
		std::uint8_t value;
		constexpr Prefix(std::uint8_t value): value(value) {}
	};
	template <class T> struct Immediate {
		T value;
		constexpr Immediate(T value): value(value) {}
	};
	void encode(Opcode opcode) {
		write<std::uint8_t>(opcode.value);
	}
	template <class T> void encode(Opcode opcode, Immediate<T> imm) {
		encode(opcode);
		write<T>(imm.value);
	}
	void encode(Opcode opcode, Register64 op1) {
		REX(1, 0, 0, op1 >> 3);
		write<std::uint8_t>(opcode.value | (op1 & 7));
	}
	template <class T> void encode(Opcode opcode, Register64 op1, Immediate<T> imm) {
		encode(opcode, op1);
		write<T>(imm.value);
	}
	void encode(Opcode opcode, std::uint8_t op1, Register32 op2) {
		REX(0, op1 >> 3, 0, op2 >> 3);
		encode(opcode);
		RM(op1, op2);
	}
	void encode(Opcode opcode, std::uint8_t op1, Register64 op2) {
		REX(1, op1 >> 3, 0, op2 >> 3);
		encode(opcode);
		RM(op1, op2);
	}
	template <class T> void encode(Opcode opcode, std::uint8_t op1, Register64 op2, Immediate<T> imm) {
		encode(opcode, op1, op2);
		write<T>(imm.value);
	}
	void encode(Opcode opcode, std::uint8_t op1, Address op2) {
		REX(1, op1 >> 3, op2.index >> 3, op2.base == -1 ? 0 : op2.base >> 3);
		encode(opcode);
		RM(op1, op2);
	}
	template <class... A> void encode(Prefix prefix, A&&... a) {
		write<std::uint8_t>(prefix.value);
		encode(std::forward<A>(a)...);
	}
public:
	std::size_t get_position() const {
		return data.size();
	}
	const std::vector<char>& get_data() const {
		return data;
	}
	void MOV(Register64 dst, Register64 src) {
		encode(Opcode(0x8B), dst, src);
	}
	void MOV(Register64 dst, Address src) {
		encode(Opcode(0x8B), dst, src);
	}
	void MOV(Address dst, Register64 src) {
		encode(Opcode(0x89), src, dst);
	}
	void MOV(Register64 dst, std::uint64_t imm) {
		encode(Opcode(0xB8), dst, Immediate<std::uint64_t>(imm));
	}
	void SYSCALL() {
		encode(Prefix(0x0F), Opcode(0x05));
	}
};

}
