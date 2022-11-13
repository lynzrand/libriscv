#include "machine.hpp"
#include "decoder_cache.hpp"
#include "riscvbase.hpp"
#include "rv32i_instr.hpp"
#include "rv32i.hpp"
#include "rv64i.hpp"
#include "rv128i.hpp"

#define INSTRUCTION_LOGGING(cpu)	\
	if ((cpu).machine().verbose_instructions) { \
		const auto string = isa_type<W>::to_string(cpu, instruction, decode(instruction)) + "\n"; \
		(cpu).machine().print(string.c_str(), string.size()); \
	}

namespace riscv
{
	[[maybe_unused]] static constexpr bool VERBOSE_FASTSIM = false;

	// Instructions may be unaligned with C-extension
	// On amd64 we take the cost, because it's faster
	union UnderAlign32 {
		uint16_t data[2];
		operator uint32_t() {
			return data[0] | uint32_t(data[1]) << 16;
		}
	};

	template <int W>
	CPU<W>::CPU(Machine<W>& machine, unsigned cpu_id, const Machine<W>& other)
		: m_machine { machine }, m_cpuid { cpu_id }
	{
		this->m_exec_data  = other.cpu.m_exec_data;
		this->m_exec_begin = other.cpu.m_exec_begin;
		this->m_exec_end   = other.cpu.m_exec_end;
		// Copy all registers except vectors
		// Users can still copy vector registers by assigning to registers().rvv().
		this->registers().copy_from(Registers<W>::Options::NoVectors, other.cpu.registers());
#ifdef RISCV_EXT_ATOMICS
		this->m_atomics = other.cpu.m_atomics;
#endif
	}
	template <int W>
	void CPU<W>::reset()
	{
		this->m_regs = {};
		this->reset_stack_pointer();
		// We can't jump if there's been no ELF loader
		if (!machine().memory.binary().empty()) {
			const auto initial_pc = machine().memory.start_address();
#ifndef RISCV_INBOUND_JUMPS_ONLY
			// Validate that the initial PC is executable.
			// Inbound jumps feature does not allow other execute areas.
			// When execute-only is active, there is no reachable execute pages.
			const auto& page =
				machine().memory.get_exec_pageno(initial_pc / riscv::Page::size());
			if (UNLIKELY(!page.attr.exec)) {
				trigger_exception(EXECUTION_SPACE_PROTECTION_FAULT, initial_pc);
			}
#endif
			// This function will (at most) validate the execute segment
			this->jump(initial_pc);
		}
		// reset the page cache
		this->m_cache = {};
	}

	template <int W>
	void CPU<W>::init_execute_area(const void* vdata, address_t begin, address_t length)
	{
		const auto* data = (const uint8_t *)vdata;
		this->initialize_exec_segs(data - begin, begin, length);
	#ifdef RISCV_INSTR_CACHE
		machine().memory.generate_decoder_cache({}, begin, begin, length);
	#endif
	}

	template <int W> __attribute__((noinline)) RISCV_INTERNAL
	typename CPU<W>::format_t CPU<W>::read_next_instruction_slowpath() const
	{
		// Fallback: Read directly from page memory
		const auto pageno = this->pc() >> Page::SHIFT;
		// Page cache
		auto& entry = this->m_cache;
		if (entry.pageno != pageno || entry.page == nullptr) {
			auto e = decltype(m_cache){pageno, &machine().memory.get_exec_pageno(pageno)};
			if (UNLIKELY(!e.page->attr.exec)) {
				trigger_exception(EXECUTION_SPACE_PROTECTION_FAULT, this->pc());
			}
			// delay setting entry until we know it's good!
			entry = e;
		}
		const auto& page = *entry.page;
		const auto offset = this->pc() & (Page::size()-1);
		format_t instruction;

		if (LIKELY(offset <= Page::size()-4)) {
			instruction.whole = uint32_t(*(UnderAlign32 *)(page.data() + offset));
			return instruction;
		}
		// It's not possible to jump to a misaligned address,
		// so there is necessarily 16-bit left of the page now.
		instruction.whole = *(uint16_t*) (page.data() + offset);

		// If it's a 32-bit instruction at a page border, we need
		// to get the next page, and then read the upper half
		if (UNLIKELY(instruction.is_long()))
		{
			const auto& page = machine().memory.get_exec_pageno(pageno+1);
			instruction.half[1] = *(uint16_t*) page.data();
		}

		return instruction;
	}

	template <int W>
	typename CPU<W>::format_t CPU<W>::read_next_instruction() const
	{
		if (LIKELY(this->is_executable(this->pc()))) {
			return format_t { *(uint32_t*) &m_exec_data[this->pc()] };
		}

#if defined(RISCV_FLAT_MEMORY) || defined(RISCV_INBOUND_JUMPS_ONLY)
		trigger_exception(EXECUTION_SPACE_PROTECTION_FAULT, this->pc());
#else
		return read_next_instruction_slowpath();
#endif
	}

	template<int W> __attribute__((hot, no_sanitize("undefined")))
	void CPU<W>::simulate_precise(uint64_t max)
	{
#ifdef RISCV_INSTR_CACHE
		auto* exec_decoder = machine().memory.get_decoder_cache();
		auto* exec_seg_data = this->m_exec_data;
#endif
		// Calculate the instruction limit
		if (max != UINT64_MAX)
			machine().set_max_instructions(machine().instruction_counter() + max);
		else
			machine().set_max_instructions(UINT64_MAX);

		for (; machine().instruction_counter() < machine().max_instructions();
			machine().increment_counter(1)) {

			format_t instruction;
#ifdef RISCV_DEBUG
			this->break_checks();
#endif

# ifdef RISCV_INSTR_CACHE
#  ifndef RISCV_INBOUND_JUMPS_ONLY
		if (LIKELY(this->is_executable(this->pc()))) {
#  endif
			auto pc = this->pc();

			// Instructions may be unaligned with C-extension
			// On amd64 we take the cost, because it's faster
#    if defined(RISCV_EXT_COMPRESSED) && !defined(__x86_64__)
			instruction = format_t { *(UnderAlign32*) &exec_seg_data[pc] };
#    else  // aligned/unaligned loads
			instruction = format_t { *(uint32_t*) &exec_seg_data[pc] };
#    endif // aligned/unaligned loads

			// Retrieve handler directly from the instruction handler cache
			auto& cache_entry =
				exec_decoder[pc / DecoderCache<W>::DIVISOR];
		#ifdef RISCV_DEBUG
			INSTRUCTION_LOGGING(*this);
		#endif
			cache_entry.execute(*this, instruction);
#   ifndef RISCV_INBOUND_JUMPS_ONLY
		} else {
			instruction = read_next_instruction_slowpath();
	#ifdef RISCV_DEBUG
			INSTRUCTION_LOGGING(*this);
	#endif
			// decode & execute instruction directly
			this->execute(instruction);
		}
#   endif // RISCV_INBOUND_JUMPS_ONLY
# else // RISCV_INSTR_CACHE
			instruction = this->read_next_instruction();
	#ifdef RISCV_DEBUG
			INSTRUCTION_LOGGING(*this);
	#endif
			// decode & execute instruction directly
			this->execute(instruction);
# endif // RISCV_INSTR_CACHE

#ifdef RISCV_DEBUG
			if (UNLIKELY(machine().verbose_registers)) {
				this->register_debug_logging();
			}
#endif
			// increment PC
			if constexpr (compressed_enabled)
				registers().pc += instruction.length();
			else
				registers().pc += 4;
		} // while not stopped

	} // CPU::simulate_precise

#ifndef RISCV_FAST_SIMULATOR
	template<int W> __attribute__((hot))
	void CPU<W>::simulate(uint64_t imax)
	{
		simulate_precise(imax);
	}

#else // RISCV_FAST_SIMULATOR

	// In fastsim mode the instruction counter becomes a register
	// the function, and we only m_counter in Machine on exit
	template <int W>
	struct InstrCounter {
		InstrCounter(Machine<W>& m) : machine(m), m_counter{m.instruction_counter()} {}
		~InstrCounter() { machine.set_instruction_counter(m_counter); }

		uint64_t value() const noexcept { return m_counter; }
		void increment_counter(uint64_t cnt) { m_counter += cnt; }
		bool overflowed() const noexcept { return m_counter > machine.max_instructions(); }
	private:
		Machine<W>& machine;
		uint64_t m_counter;
	};

	template<int W> __attribute__((hot))
	void CPU<W>::simulate(uint64_t imax)
	{
		auto* exec_decoder = machine().memory.get_decoder_cache();

		InstrCounter counter{machine()};

		if (imax != UINT64_MAX)
			machine().set_max_instructions(counter.value() + imax);
		else
			machine().set_max_instructions(UINT64_MAX);

		auto pc = this->pc();
		do {
			// Retrieve handler directly from the instruction handler cache
			auto* decoder = &exec_decoder[pc / DecoderCache<W>::DIVISOR];
			// The number of instructions to run until we can check
			// if we ran out of instructions or PC changed.
			size_t count = decoder->idxend;
			if constexpr (VERBOSE_FASTSIM) {
				printf("Fastsim at PC=0x%lX count=%zu\n", (long)pc, count);
			}
			size_t instr_count;
			if constexpr (!compressed_enabled)
				instr_count = count;
			else
				instr_count = decoder->idxend - decoder->instr_count;
			counter.increment_counter(instr_count);
			auto* decoder_end = &decoder[count];
			unsigned length = 0;
			// We want to run 4 instructions at a time, except for
			// the last one, which we will "always" do next
			if constexpr (!compressed_enabled)
			{
				while (decoder + 4 < decoder_end)
				{
					registers().pc = pc + 0;
					decoder[0].execute(*this);
					registers().pc = pc + 4;
					decoder[1].execute(*this);
					registers().pc = pc + 8;
					decoder[2].execute(*this);
					registers().pc = pc + 12;
					decoder[3].execute(*this);
					pc += 16;
					decoder += 4;
				}
			} else { // Conservative compressed version
				while (decoder + 4 < decoder_end)
				{
					// Here we try to avoid re-reading decoder memory
					// after executing instructions, by using locals.
					auto* decoder0 = decoder;
					const auto oplen0 = decoder0->opcode_length;
					auto* decoder1 = decoder0 + oplen0 / 2;
					const auto oplen1 = decoder1->opcode_length;

					registers().pc = pc;
					decoder0->execute(*this);

					pc += oplen0;

					registers().pc = pc;
					decoder1->execute(*this);

					pc += oplen1;
					decoder = decoder1 + oplen1 / 2;
				}
			}
			// There is always one instruction we can run
			do {
				// Some instructions use PC offsets
				registers().pc = pc;
				// Debugging aid when fast simulator is behaving strangely
				if constexpr (VERBOSE_FASTSIM) {
					const format_t instruction{decoder->instr};
					const auto string = isa_type<W>::to_string(*this, instruction, decode(instruction)) + "\n";
					machine().print(string.c_str(), string.size());
				}
				// Execute instruction using handler and 32-bit wrapper
				decoder->execute(*this);
				// increment *local* PC
				if constexpr (compressed_enabled) {
					length = decoder->opcode_length;
					pc += length;
					decoder += length / 2;
				} else {
					pc += 4;
					decoder++;
				}
			} while (decoder < decoder_end);
			// The loop above ended because PC could have changed.
			// Update to real PC by reading from register memory.
			if constexpr (compressed_enabled)
				pc = registers().pc + length;
			else
				pc = registers().pc + 4;

		} while (!counter.overflowed());
		registers().pc = pc;
	} // CPU::simulate
#endif // RISCV_FAST_SIMULATOR

	template<int W>
	void CPU<W>::step_one()
	{
		// Read, decode & execute instructions directly
		auto instruction = this->read_next_instruction();
		this->execute(instruction);

		if constexpr (compressed_enabled)
			registers().pc += instruction.length();
		else
			registers().pc += 4;
	}

	template<int W> __attribute__((cold))
	void CPU<W>::trigger_exception(int intr, address_t data)
	{
		switch (intr)
		{
		case ILLEGAL_OPCODE:
			throw MachineException(ILLEGAL_OPCODE,
					"Illegal opcode executed", data);
		case ILLEGAL_OPERATION:
			throw MachineException(ILLEGAL_OPERATION,
					"Illegal operation during instruction decoding", data);
		case PROTECTION_FAULT:
			throw MachineException(PROTECTION_FAULT,
					"Protection fault", data);
		case EXECUTION_SPACE_PROTECTION_FAULT:
			throw MachineException(EXECUTION_SPACE_PROTECTION_FAULT,
					"Execution space protection fault", data);
		case MISALIGNED_INSTRUCTION:
			// NOTE: only check for this when jumping or branching
			throw MachineException(MISALIGNED_INSTRUCTION,
					"Misaligned instruction executed", data);
		case INVALID_ALIGNMENT:
			throw MachineException(INVALID_ALIGNMENT,
					"Invalid alignment for address", data);
		case UNIMPLEMENTED_INSTRUCTION:
			throw MachineException(UNIMPLEMENTED_INSTRUCTION,
					"Unimplemented instruction executed", data);
		case DEADLOCK_REACHED:
			throw MachineException(DEADLOCK_REACHED,
					"Atomics deadlock reached", data);
		default:
			throw MachineException(UNKNOWN_EXCEPTION,
					"Unknown exception", intr);
		}
	}

	template <int W> __attribute__((cold))
	std::string CPU<W>::to_string(format_t format, const instruction_t& instr) const
	{
		if constexpr (W == 4)
			return RV32I::to_string(*this, format, instr);
		else if constexpr (W == 8)
			return RV64I::to_string(*this, format, instr);
		else if constexpr (W == 16)
			return RV128I::to_string(*this, format, instr);
		return "Unknown architecture";
	}

	template <int W> __attribute__((cold))
	std::string CPU<W>::current_instruction_to_string() const
	{
		format_t instruction;
		try {
			instruction = this->read_next_instruction();
		} catch (...) {
			instruction = format_t {};
		}
		return isa_type<W>::to_string(*this, instruction, decode(instruction));
	}

	template <int W> __attribute__((cold))
	std::string Registers<W>::flp_to_string() const
	{
		char buffer[800];
		int  len = 0;
		for (int i = 0; i < 32; i++) {
			auto& src = this->getfl(i);
			const char T = (src.i32[1] == -1) ? 'S' : 'D';
			double val = (src.i32[1] == -1) ? src.f32[0] : src.f64;
			len += snprintf(buffer+len, sizeof(buffer) - len,
					"[%s\t%c%+.2f] ", RISCV::flpname(i), T, val);
			if (i % 5 == 4) {
				len += snprintf(buffer+len, sizeof(buffer)-len, "\n");
			}
		}
		return std::string(buffer, len);
	}

	template struct CPU<4>;
	template struct Registers<4>;
	template struct CPU<8>;
	template struct Registers<8>;
	template struct CPU<16>;
	template struct Registers<16>;
}
