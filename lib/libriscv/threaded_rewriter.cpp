#include "machine.hpp"
#include "threaded_bytecodes.hpp"

#include "instruction_list.hpp"
#include "rv32i_instr.hpp"
#include "rvc.hpp"

namespace riscv
{
	template <int W> RISCV_INTERNAL
	size_t CPU<W>::threaded_rewrite(size_t bytecode, address_t pc, rv32i_instruction& instr)
	{
		static constexpr unsigned PCAL = compressed_enabled ? 2 : 4;
		const auto original = instr;
		(void) pc;

		switch (bytecode)
		{
			case RV32I_BC_LI: {
				FasterItype rewritten;
				rewritten.rs1 = original.Itype.rd;
				rewritten.rs2 = 0;
				rewritten.imm = original.Itype.signed_imm();

				instr.whole = rewritten.whole;
				return bytecode;
			}
			case RV32I_BC_ADDI:
			case RV32I_BC_SLLI:
			case RV32I_BC_SLTI:
			case RV32I_BC_SLTIU:
			case RV32I_BC_XORI:
			case RV32I_BC_SRLI:
			case RV32I_BC_SRAI:
			case RV32I_BC_ORI:
			case RV32I_BC_ANDI: {
				FasterItype rewritten;
				rewritten.rs1 = original.Itype.rd;
				rewritten.rs2 = original.Itype.rs1;
				rewritten.imm = original.Itype.signed_imm();

				instr.whole = rewritten.whole;
				return bytecode;
			}
			case RV32I_BC_BEQ:
			case RV32I_BC_BNE:
			case RV32I_BC_BLT:
			case RV32I_BC_BGE:
			case RV32I_BC_BLTU:
			case RV32I_BC_BGEU: {
				FasterItype rewritten;
				rewritten.rs1 = original.Btype.rs1;
				rewritten.rs2 = original.Btype.rs2;
				rewritten.imm = original.Btype.signed_imm();

				instr.whole = rewritten.whole;
				return bytecode;
			}
			case RV32I_BC_OP_ADD:
			case RV32I_BC_OP_SUB:
			case RV32I_BC_OP_SLL:
			case RV32I_BC_OP_SLT:
			case RV32I_BC_OP_SLTU:
			case RV32I_BC_OP_XOR:
			case RV32I_BC_OP_SRL:
			case RV32I_BC_OP_SRA:
			case RV32I_BC_OP_OR:
			case RV32I_BC_OP_AND:
			case RV32I_BC_OP_MUL:
			case RV32I_BC_OP_MULH:
			case RV32I_BC_OP_MULHSU:
			case RV32I_BC_OP_MULHU:
			case RV32I_BC_OP_DIV:
			case RV32I_BC_OP_DIVU:
			case RV32I_BC_OP_REM:
			case RV32I_BC_OP_REMU:
			case RV32I_BC_OP_SH1ADD:
			case RV32I_BC_OP_SH2ADD:
			case RV32I_BC_OP_SH3ADD: {
				FasterOpType rewritten;
				rewritten.rd = original.Rtype.rd;
				rewritten.rs1 = original.Rtype.rs1;
				rewritten.rs2 = original.Rtype.rs2;

				instr.whole = rewritten.whole;
				return bytecode;
			}
			case RV32I_BC_LDW: {
				FasterItype rewritten;
				rewritten.rs1 = original.Itype.rd;
				rewritten.rs2 = original.Itype.rs1;
				rewritten.imm = original.Itype.signed_imm();

				instr.whole = rewritten.whole;
				return bytecode;
			}
			case RV32I_BC_STW: {
				FasterItype rewritten;
				rewritten.rs1 = original.Stype.rs1;
				rewritten.rs2 = original.Stype.rs2;
				rewritten.imm = original.Stype.signed_imm();

				instr.whole = rewritten.whole;
				return bytecode;
			}
			case RV32I_BC_JAL: {
				// Here we try to find out if the whole jump
				// can be expressed as just the instruction bits.
				const auto addr = pc + original.Jtype.jump_offset();
				const bool is_aligned = addr % PCAL == 0;
				const bool below32 = addr < UINT32_MAX;
				const bool store_zero = original.Jtype.rd == 0;

				if (is_aligned && below32 && store_zero)
				{
					instr.whole = addr;
					return RV32I_BC_FAST_JAL;
				}

				FasterJtype rewritten;
				rewritten.offset = original.Jtype.jump_offset();
				rewritten.rd     = original.Jtype.rd;

				instr.whole = rewritten.whole;
				return bytecode;
			}
			/** Compressed instructions **/
#ifdef RISCV_EXT_COMPRESSED
			case RV32C_BC_ADDI: {
				const rv32c_instruction ci{original};

				FasterItype rewritten;
				rewritten.rs1 = ci.CI.rd;
				rewritten.rs2 = ci.CI.rd;
				rewritten.imm = ci.CI.signed_imm();

				instr.whole = rewritten.whole;
				return RV32C_BC_ADDI;
			}
			case RV32C_BC_LI: {
				const rv32c_instruction ci{original};

				FasterItype rewritten;
				rewritten.rs1 = ci.CI.rd;
				rewritten.rs2 = 0;
				rewritten.imm = ci.CI.signed_imm();

				instr.whole = rewritten.whole;
				return RV32C_BC_ADDI;
			}
			case RV32C_BC_MV: {
				const rv32c_instruction ci{original};

				FasterItype rewritten;
				rewritten.rs1 = ci.CR.rd;
				rewritten.rs2 = ci.CR.rs2;
				rewritten.imm = 0;

				instr.whole = rewritten.whole;
				return RV32C_BC_ADDI;
			}
#endif // RISCV_EXT_COMPRESSED
		}

		return bytecode;
	}

	template struct CPU<4>;
	template struct CPU<8>;
	template struct CPU<16>;
} // riscv