# The patches (*.c here), compiled for MIPS against the decomp's headers (lib/papermario) as its context builds read
# them (__CTX__ leaves out the assembly its headers include), and linked into patches.elf, which N64Recomp recompiles
# with patches.toml (CMakeLists.txt's PatchesLib). dummy_headers/ stands in for the headers the decomp's build
# generates from the ROM and for KMC gcc's stdarg.h (pm_stdarg.h says why). CC is a clang with a MIPS backend and LD
# ld.lld.
TARGET = patches.elf

CC      ?= clang
LD      ?= ld.lld

CFLAGS   := -target mips -mips2 -mabi=32 -O2 -G0 -mno-abicalls -mno-odd-spreg -mno-check-zero-division \
			-fomit-frame-pointer -ffast-math -fno-unsafe-math-optimizations -fno-builtin-memset \
			-Wall -Wextra -Wno-incompatible-library-redeclaration -Wno-unused-parameter -Wno-unknown-pragmas \
			-Wno-unused-variable -Wno-unused-but-set-variable -Wno-missing-braces -Wno-unsupported-floating-point-opt -Wno-shift-negative-value
CPPFLAGS := -nostdinc -D_LANGUAGE_C -DLANGUAGE_C -DMIPS -D__sgi -D_MIPS_SZLONG=32 -DF3DEX_GBI_2 -DNDEBUG -DN_MICRO 			-D_FINALROM -DVERSION_US -D__CTX__ 			-I dummy_headers -I ../lib/papermario/include -I ../lib/papermario/include/PR 			-I ../lib/papermario/include/libc -I ../lib/papermario/src -I ../lib/rt64/include
LDFLAGS  := -nostdlib -T patches.ld -T syms.ld -Map patches.map --unresolved-symbols=ignore-all --emit-relocs

C_SRCS := $(wildcard *.c)
C_OBJS := $(C_SRCS:.c=.o)
C_DEPS := $(C_SRCS:.c=.d)

$(TARGET): $(C_OBJS) patches.ld syms.ld
	$(LD) $(C_OBJS) $(LDFLAGS) -o $@

$(C_OBJS): %.o : %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $< -MMD -MF $(@:.o=.d) -c -o $@

clean:
	rm -rf $(C_OBJS) $(TARGET) $(C_DEPS)

-include $(C_DEPS)

.PHONY: clean
