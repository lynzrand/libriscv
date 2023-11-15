#include <libriscv/machine.hpp>
#include <chrono>
#include <fstream>
#include <iterator>
#define TIME_POINT(t) \
	asm("" ::: "memory"); \
	auto t = std::chrono::high_resolution_clock::now(); \
	asm("" ::: "memory");
namespace {
	extern const std::array<unsigned char, 900> fib_elf;
}

int main(int argc, char **argv)
{
	// Default: fib(256000000)
	std::string_view binview{(const char *)fib_elf.data(), fib_elf.size() };
	std::vector<char> binary;

	// Program argument: Read and execute the provided file
	if (argc > 1) {
		const char* filename = argv[1];

		std::ifstream fs(filename, std::ios::binary);
		if (!fs) {
			fprintf(stderr, "Not able to access %s!\n", filename);
		}
		binary.assign(std::istreambuf_iterator<char>(fs), std::istreambuf_iterator<char>());

		binview = { binary.data(), binary.size() };
	}

	// Setup a machine
	riscv::Machine<riscv::RISCV32> machine{ binview };
	machine.setup_minimal_syscalls();
	machine.setup_argv({
			"libriscv", "Hello", "World"
		});

	TIME_POINT(t0);
	machine.simulate();
	TIME_POINT(t1);

	const std::chrono::duration<double, std::milli> exec_time = t1 - t0;
	printf("Runtime: %.3fms  MI/s: %.2f\n", exec_time.count(),
		machine.instruction_counter() / (exec_time.count() * 1e3));
}

namespace {
	/** 'fib' calculates the 256000000th fibonacci number.
00010074 <_start>:
_start():
   10074:	1141                	add	sp,sp,-16
   10076:	0f4247b7          		lui	a5,0xf424
   1007a:	c63e                	sw	a5,12(sp)
   1007c:	47b2                	lw	a5,12(sp)
   1007e:	4685                	li	a3,1
   10080:	4701                	li	a4,0
   10082:	e399                	bnez	a5,10088 <_start+0x14>
   10084:	a829                	j	1009e <_start+0x2a>
   10086:	872a                	mv	a4,a0
   10088:	17fd                	add	a5,a5,-1 # f423fff <__global_pointer$+0xf412753>
   1008a:	00d70533          		add	a0,a4,a3
   1008e:	86ba                	mv	a3,a4
   10090:	fbfd                	bnez	a5,10086 <_start+0x12>
   10092:	05d00893          		li	a7,93
   10096:	00000073          		ecall
   1009a:	0141                	add	sp,sp,16
   1009c:	8082                	ret
   1009e:	4501                	li	a0,0
   100a0:	05d00893          		li	a7,93
   100a4:	00000073          		ecall
   100a8:	0141                	add	sp,sp,16
   100aa:	8082                	ret
	**/
	static constexpr std::array<unsigned char, 900> fib_elf {
		0x7f, 0x45, 0x4c, 0x46, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0xf3, 0x00, 0x01, 0x00, 0x00, 0x00,
		0x74, 0x00, 0x01, 0x00, 0x34, 0x00, 0x00, 0x00, 0x6c, 0x02, 0x00, 0x00,
		0x05, 0x00, 0x00, 0x00, 0x34, 0x00, 0x20, 0x00, 0x02, 0x00, 0x28, 0x00,
		0x07, 0x00, 0x06, 0x00, 0x03, 0x00, 0x00, 0x70, 0xbb, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x35, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
		0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
		0x00, 0x00, 0x01, 0x00, 0xac, 0x00, 0x00, 0x00, 0xac, 0x00, 0x00, 0x00,
		0x05, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x41, 0x11, 0xb7, 0x47,
		0x42, 0x0f, 0x3e, 0xc6, 0xb2, 0x47, 0x85, 0x46, 0x01, 0x47, 0x99, 0xe3,
		0x29, 0xa8, 0x2a, 0x87, 0xfd, 0x17, 0x33, 0x05, 0xd7, 0x00, 0xba, 0x86,
		0xfd, 0xfb, 0x93, 0x08, 0xd0, 0x05, 0x73, 0x00, 0x00, 0x00, 0x41, 0x01,
		0x82, 0x80, 0x01, 0x45, 0x93, 0x08, 0xd0, 0x05, 0x73, 0x00, 0x00, 0x00,
		0x41, 0x01, 0x82, 0x80, 0x47, 0x43, 0x43, 0x3a, 0x20, 0x28, 0x29, 0x20,
		0x31, 0x32, 0x2e, 0x32, 0x2e, 0x30, 0x00, 0x41, 0x34, 0x00, 0x00, 0x00,
		0x72, 0x69, 0x73, 0x63, 0x76, 0x00, 0x01, 0x2a, 0x00, 0x00, 0x00, 0x04,
		0x10, 0x05, 0x72, 0x76, 0x33, 0x32, 0x69, 0x32, 0x70, 0x30, 0x5f, 0x6d,
		0x32, 0x70, 0x30, 0x5f, 0x61, 0x32, 0x70, 0x30, 0x5f, 0x66, 0x32, 0x70,
		0x30, 0x5f, 0x64, 0x32, 0x70, 0x30, 0x5f, 0x63, 0x32, 0x70, 0x30, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x74, 0x00, 0x01, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x02, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x03, 0x00, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0xf1, 0xff, 0x07, 0x00, 0x00, 0x00,
		0x74, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
		0x0a, 0x00, 0x00, 0x00, 0xac, 0x18, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x10, 0x00, 0xf1, 0xff, 0x1c, 0x00, 0x00, 0x00, 0xac, 0x10, 0x01, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x01, 0x00, 0x3d, 0x00, 0x00, 0x00,
		0x74, 0x00, 0x01, 0x00, 0x38, 0x00, 0x00, 0x00, 0x12, 0x00, 0x01, 0x00,
		0x2c, 0x00, 0x00, 0x00, 0xac, 0x10, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x10, 0x00, 0x01, 0x00, 0x38, 0x00, 0x00, 0x00, 0xac, 0x10, 0x01, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x01, 0x00, 0x44, 0x00, 0x00, 0x00,
		0xac, 0x10, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x01, 0x00,
		0x53, 0x00, 0x00, 0x00, 0xac, 0x10, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x10, 0x00, 0x01, 0x00, 0x5a, 0x00, 0x00, 0x00, 0xac, 0x10, 0x01, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x01, 0x00, 0x00, 0x66, 0x69, 0x62,
		0x2e, 0x63, 0x00, 0x24, 0x78, 0x00, 0x5f, 0x5f, 0x67, 0x6c, 0x6f, 0x62,
		0x61, 0x6c, 0x5f, 0x70, 0x6f, 0x69, 0x6e, 0x74, 0x65, 0x72, 0x24, 0x00,
		0x5f, 0x5f, 0x53, 0x44, 0x41, 0x54, 0x41, 0x5f, 0x42, 0x45, 0x47, 0x49,
		0x4e, 0x5f, 0x5f, 0x00, 0x5f, 0x5f, 0x42, 0x53, 0x53, 0x5f, 0x45, 0x4e,
		0x44, 0x5f, 0x5f, 0x00, 0x5f, 0x5f, 0x62, 0x73, 0x73, 0x5f, 0x73, 0x74,
		0x61, 0x72, 0x74, 0x00, 0x5f, 0x5f, 0x44, 0x41, 0x54, 0x41, 0x5f, 0x42,
		0x45, 0x47, 0x49, 0x4e, 0x5f, 0x5f, 0x00, 0x5f, 0x65, 0x64, 0x61, 0x74,
		0x61, 0x00, 0x5f, 0x65, 0x6e, 0x64, 0x00, 0x00, 0x2e, 0x73, 0x79, 0x6d,
		0x74, 0x61, 0x62, 0x00, 0x2e, 0x73, 0x74, 0x72, 0x74, 0x61, 0x62, 0x00,
		0x2e, 0x73, 0x68, 0x73, 0x74, 0x72, 0x74, 0x61, 0x62, 0x00, 0x2e, 0x74,
		0x65, 0x78, 0x74, 0x00, 0x2e, 0x63, 0x6f, 0x6d, 0x6d, 0x65, 0x6e, 0x74,
		0x00, 0x2e, 0x72, 0x69, 0x73, 0x63, 0x76, 0x2e, 0x61, 0x74, 0x74, 0x72,
		0x69, 0x62, 0x75, 0x74, 0x65, 0x73, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x1b, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00,
		0x74, 0x00, 0x01, 0x00, 0x74, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x21, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
		0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xac, 0x00, 0x00, 0x00,
		0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x2a, 0x00, 0x00, 0x00,
		0x03, 0x00, 0x00, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0xbb, 0x00, 0x00, 0x00, 0x35, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0xf0, 0x00, 0x00, 0x00, 0xe0, 0x00, 0x00, 0x00,
		0x05, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
		0x10, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xd0, 0x01, 0x00, 0x00,
		0x5f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00,
		0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x2f, 0x02, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
}
