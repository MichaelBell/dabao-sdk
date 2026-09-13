.include "macros.s"

.section .text,"ax"

# The data pins on my breakout:
# CSn - PC7 = 23 - t0
# CLK - PC3 = 19 - x8
# D0  - PC2 = 18
# D1  - PC1 = 17
# D2  - PC0 = 16
# D3 - PB14 = 14
#
# Which is a little awkward.
# Encoding/decoding from the pin format will be done by lookup table.

# Instruction timing for non-branch instructions is:
# 4 cycles for any instruction aligned on a word boundary
# 3 cycles for an unaligned compressed instruction
# 5 cycles for an unaligned uncompressed instruction
#
# Therefore, turn off compression by default, and use it explicitly on even
# numbers of consecutive compressible instructions only.
.option norvc

initialize: 
    li t0, 0x800000   # CS on pin 23
    srli x8, t0, 4    # CK on pin 19
    li x1, 0x23d      # All output mask
    slli x1, x1, 14
    li x4, 0x88       # CS clk only output mask
.option rvc
    slli x4, x4, 16
    li x3, 1
    la x9, encode_table

    # Setup IO mask
    wriomsk x1
    wrevmsk x3
    setoe   x4
    wrio    t0  # CS high, clock and data low

func_sel:
    # 0 to write, non-zero to read
    rdf2 a0
    rdf2 a1           # Length
    rdf2 a2           # PSRAM address to read/write
    rdf2 a3           # Bao address to write/read
.option norvc
    bnez a0, do_read

do_write:
    srli a0, t0, 6  # Generate command 02h
    wrio x0         # CS low
.option rvc
    setoe x1        # Enable all outputs
    setio x8        # Clock high, data 0
.option norvc
    srli a5, a2, 18
    wrio a0         # Clock low, data 2
    andi a5, a5, 0x3c
.option rvc
    setio x8        # Clock high, data 2

write_address:
.macro write_nibble src_reg next_shift
    add a5, a5, x9
    lw x21, 0(a5)   # Clock low, address 23-20
    
    srli a5, \src_reg, \next_shift
    andi a5, a5, 0x3c

    setio x8        # Clock high
.endm

    write_nibble a2 14
    write_nibble a2 10
    write_nibble a2 6
    write_nibble a2 2

    add a5, a5, x9
    lw x21, 0(a5)   # Clock low, address 7-4

    slli a5, a2, 2
    andi a5, a5, 0x3c

    setio x8        # Clock high

    add a5, a5, x9
.option norvc
    lw x21, 0(a5)   # Clock low, address 3-0

    li a4, 64
.option rvc
    setio x8        # Clock high

write_data:
    sub a1, a1, a4
.option norvc
    bgez a1, write_data_loop
    add a4, a4, a1

write_data_loop:
    lw a0, 0(a3)
    srli a5, a0, 2

    andi a5, a5, 0x3c
    add a5, a5, x9
    lw x21, 0(a5)   # Clock low, data 7-4
    
    slli a5, a0, 2
    andi a5, a5, 0x3c

.option rvc
    setio x8        # Clock high

    write_nibble a0 10
    write_nibble a0 6
    write_nibble a0 18
    write_nibble a0 14
    write_nibble a0 26
    write_nibble a0 22

    add a5, a5, x9
    lw x21, 0(a5)   # Clock low, data 27-24

    addi a3, a3, 4
    addi a4, a4, -4

    setio x8        # Clock high

    bnez a4, write_data_loop
.option norvc

    not x25, x4 # Disable data out
    wrio    t0  # CS high, clock and data low

    addi a2, a2, 64
    bgtz a1, do_write

    setev x3
    j func_sel

do_read:
    li a0, 0x64000  # Generate command 0Bh
    wrio x0         # CS low
    setoe x1        # Enable all outputs
    setio x8        # Clock high, data 0
    srli a5, a2, 18
    wrio a0         # Clock low, data B
    andi a5, a5, 0x3c
.option rvc
    setio x8        # Clock high, data B

    write_nibble a2 14
    write_nibble a2 10
    write_nibble a2 6
    write_nibble a2 2

    add a5, a5, x9
    lw x21, 0(a5)   # Clock low, address 7-4

    slli a5, a2, 2
    andi a5, a5, 0x3c

    setio x8        # Clock high

    add a5, a5, x9
.option norvc
    lw x21, 0(a5)   # Clock low, address 3-0

    li a4, 64
.option rvc
    setio x8        # Clock high

    sub a1, a1, a4
.option norvc
    li x21, 0       # Clock low, data 0
    not x25, x4     # Disable data out

    setio x8        # Clock high
    sltz a0, a1
    li x21, 0       # Clock low, data 0
    neg a0, a0
.option rvc
    setio x8        # Clock high
    and a0, a0, a1
    li x21, 0       # Clock low, data 0
    add a4, a4, a0
    setio x8        # Clock high
    nop
    li x21, 0       # Clock low, data 0
    nop
    setio x8        # Clock high
    nop
    li x21, 0       # Clock low, data 0
    nop

read_data_loop:
.option norvc
    setio x8        # Clock high
    srli a5, x21, 14
.option rvc
    andi a5, a5, 0x1f
    add a5, a5, x9
.option norvc
    li x21, 0       # Clock low, data 0
    lb a5, 64(a5)
    slli a0, a5, 4

    setio x8        # Clock high
    srli a5, x21, 14
.option rvc
    andi a5, a5, 0x1f
    add a5, a5, x9
.option norvc
    li x21, 0       # Clock low, data 0
    lb a5, 64(a5)
.option rvc
    or a0, a0, a5

.macro read_byte shift1 shift2
    setio x8        # Clock high
    srli a5, x21, 14
    andi a5, a5, 0x1f
    add a5, a5, x9
    lb a5, 64(a5)
    li x21, 0       # Clock low, data 0
    slli a5, a5, \shift1
    or a0, a0, a5

    setio x8        # Clock high
    srli a5, x21, 14
    andi a5, a5, 0x1f
    add a5, a5, x9
    lb a5, 64(a5)
    li x21, 0       # Clock low, data 0
    slli a5, a5, \shift2
    or a0, a0, a5
.endm

    read_byte 12 8
    read_byte 20 16
    read_byte 28 24

    sw a0, 0(a3)

    addi a3, a3, 4
    addi a4, a4, -4

.option norvc
    bnez a4, read_data_loop

    wrio    t0  # CS high, clock and data low
    addi a2, a2, 64
    bgtz a1, do_read

    setev x3
    j func_sel


encode_table:
.word 0x00000
.word 0x40000
.word 0x20000
.word 0x60000
.word 0x10000
.word 0x50000
.word 0x30000
.word 0x70000
.word 0x04000
.word 0x44000
.word 0x24000
.word 0x64000
.word 0x14000
.word 0x54000
.word 0x34000
.word 0x74000

decode_table:
.byte 0x00  # 000_0
.byte 0x08  # 000_1
.byte 0x00  # 000_0
.byte 0x08  # 000_1
.byte 0x04  # 001_0
.byte 0x0c  # 001_1
.byte 0x04  # 001_0
.byte 0x0c  # 001_1
.byte 0x02  # 010_0
.byte 0x0a  # 010_1
.byte 0x02  # 010_0
.byte 0x0a  # 010_1
.byte 0x06  # 011_0
.byte 0x0e  # 011_1
.byte 0x06  # 011_0
.byte 0x0e  # 011_1
.byte 0x01  # 100_0
.byte 0x09  # 100_1
.byte 0x01  # 100_0
.byte 0x09  # 100_1
.byte 0x05  # 101_0
.byte 0x0e  # 101_1
.byte 0x05  # 101_0
.byte 0x0e  # 101_1
.byte 0x03  # 110_0
.byte 0x0b  # 110_1
.byte 0x03  # 110_0
.byte 0x0b  # 110_1
.byte 0x07  # 111_0
.byte 0x0f  # 111_1
.byte 0x07  # 111_0
.byte 0x0f  # 111_1
