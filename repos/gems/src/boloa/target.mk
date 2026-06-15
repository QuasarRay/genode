# loader runs in protected mode, which requires 32bit code
REQUIRES = x86_32

TARGET = boloa

SRC_CC += entry.cc log.cc misc.cc
SRC_CC += elf_binary.cc output.cc hid.cc

SRC_C  += unwind.c

SRC_S  += crt0.s

SRC_BIN = mono.tff

INC_DIR += $(BASE_DIR)/src/include

CC_CXX_OPT += -fno-rtti
CC_OPT     += -mgeneral-regs-only

# source code requires multiple of 16M for LD_TEXT_ADDR
LD_TEXT_ADDR = 0x02000000
CC_OPT_crt0 += -xassembler-with-cpp -DLD_TEXT_ADDR=$(LD_TEXT_ADDR)

vpath hid.cc        $(BASE_DIR)/src/lib/base
vpath output.cc     $(BASE_DIR)/src/lib/base
vpath unwind.c      $(BASE_DIR)/src/lib/cxx
