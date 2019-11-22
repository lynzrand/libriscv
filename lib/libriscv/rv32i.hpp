#pragma once
#include <array>
#include <cstdint>
#include <string>
#include "types.hpp"
#include "rv32c.hpp"

namespace riscv
{
	union rv32c_instruction;

	union rv32i_instruction
	{
		using word_t = uint32_t;
		using sword_t = int32_t;

		// register format
		struct {
			uint32_t opcode : 7;
			uint32_t rd     : 5;
			uint32_t funct3 : 3;
			uint32_t rs1    : 5;
			uint32_t rs2    : 5;
			uint32_t funct7 : 7;

			bool is_f7() const noexcept {
				return funct7 == 0b0100000;
			}
			bool is_32M() const noexcept {
				return funct7 == 0b0000001;
			}
			word_t jumptable_friendly_op() const noexcept {
				// use bit 4 for RV32M extension
				return funct3 | ((funct7 & 1) << 4);
			}
		} Rtype;
		// immediate format
		struct {
			uint32_t opcode : 7;
			uint32_t rd     : 5;
			uint32_t funct3 : 3;
			uint32_t rs1    : 5;
			uint32_t imm    : 12;

			bool sign() const noexcept {
				return imm & 0x800;
			}
			int32_t signed_imm() const noexcept {
				const uint32_t ext = 0xFFFFF000;
				return imm | (sign() ? ext : 0);
			}
			uint32_t shift_imm() const noexcept {
				return imm & 0x1F;
			}
			bool is_srai() const noexcept {
				return imm & 0x400;
			}
		} Itype;
		// store format
		struct {
			uint32_t opcode : 7;
			uint32_t imm1   : 5;
			uint32_t funct3 : 3;
			uint32_t rs1    : 5;
			uint32_t rs2    : 5;
			uint32_t imm2   : 7;

			bool sign() const noexcept {
				return imm2 & 0x40;
			}
			int32_t signed_imm() const noexcept {
				const uint32_t ext = 0xFFFFF000;
				return imm1 | (imm2 << 5) | (sign() ? ext : 0);
			}
		} Stype;
		// upper immediate format
		struct {
			uint32_t opcode : 7;
			uint32_t rd     : 5;
			uint32_t imm    : 20;

			bool sign() const noexcept {
				return imm & 0x80000;
			}
			int32_t signed_imm() const noexcept {
				const uint32_t ext = 0xFFF00000;
				return imm | (sign() ? ext : 0);
			}
			uint32_t upper_imm() const noexcept {
				return imm << 12u;
			}
		} Utype;
		// branch type
		struct {
			uint32_t opcode : 7;
			uint32_t imm1   : 1;
			uint32_t imm2   : 4;
			uint32_t funct3 : 3;
			uint32_t rs1    : 5;
			uint32_t rs2    : 5;
			uint32_t imm3   : 6;
			uint32_t imm4   : 1;

			bool sign() const noexcept {
				return imm4;
			}
			int32_t signed_imm() const noexcept {
				const uint32_t ext = 0xFFFFF000;
				return (imm2 << 1) | (imm3 << 5) | (imm1 << 11) | (sign() ? ext : 0);
			}
		} Btype;
		// jump instructions
		struct {
			uint32_t opcode : 7;
			uint32_t rd     : 5;
			uint32_t imm1   : 8;
			uint32_t imm2   : 1;
			uint32_t imm3   : 10;
			uint32_t imm4   : 1;

			bool sign() const noexcept {
				return imm4;
			}
			int32_t jump_offset() const noexcept {
				const int32_t  jo  = (imm3 << 1) | (imm2 << 11) | (imm1 << 12);
				const uint32_t ext = 0xFFF00000;
				return jo | (sign() ? ext : 0);
			}
		} Jtype;
		// atomic format
		struct {
			uint32_t opcode : 7;
			uint32_t rd     : 5;
			uint32_t funct3 : 3;
			uint32_t rs1    : 5;
			uint32_t rs2    : 5;
			uint32_t rl     : 1;
			uint32_t aq     : 1;
			uint32_t funct5 : 5;
		} Atype;

		uint16_t half[2];
		uint32_t whole;

		rv32i_instruction() : whole(0) {}
		rv32i_instruction(uint32_t another) : whole(another) {}

		uint32_t opcode() const noexcept {
			return Rtype.opcode;
		}

		uint32_t length() const noexcept {
			return ((Rtype.opcode & 0x3) == 0x3) ? 4 : 2;
		}
		bool is_long() const noexcept {
			return (whole & 0x3) == 0x3;
		}

		inline auto compressed() const noexcept {
			return rv32c_instruction { half[0] };
		}
		inline auto fpfunc() const noexcept {
			return whole >> (32 - 5);
		}

		static constexpr sword_t to_signed(word_t word) noexcept {
			return (sword_t) word;
		}
		template <typename T>
		static constexpr word_t to_word(T x) noexcept {
			return (word_t) x;
		}
	};
	static_assert(sizeof(rv32i_instruction) == 4, "Instruction is 4 bytes");

	struct RV32I {
		using address_t     = uint32_t; // ??
		using format_t      = rv32i_instruction;
		using compressed_t  = rv32c_instruction;
		using instruction_t = Instruction<4>;
		using register_t    = uint32_t;

		static std::string to_string(CPU<4>& cpu, format_t format, const instruction_t& instr);

		static inline uint32_t SRA(bool is_signed, uint32_t shifts, uint32_t value)
		{
			const uint32_t sign_bits = -is_signed ^ 0x0;
			const uint32_t sign_shifted = sign_bits << (32 - shifts);
			return (value >> shifts) | sign_shifted;
		}
	};
}
