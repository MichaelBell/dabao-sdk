.include "macros.s"

.section .text,"ax"

# Instruction timing for non-branch instructions is:
# 4 cycles for any instruction aligned on a word boundary
# 3 cycles for an unaligned compressed instruction
# 5 cycles for an unaligned uncompressed instruction
#
# Therefore, turn off compression by default, and use it explicitly on even
# numbers of consecutive compressible instructions only.
.option norvc

# SPI send and receive
# Pinout:
# - CS PC7 = 23 t0
# - CK PC3 = 19 x8
# - CO PC2 = 18 x4 (Controller out)
# - CI PC1 = 17 a4 (Controller in)

# Data length and addresses read from FIFO 2:
# - Length in bytes
# - Read address
# - Write address

# Byte transfer at ~30.4MHz (clock high 12 cycles, low 11 cycles for each bit)

_start: 
    li t0, 0x800000   # CS on pin 23
    srli x8, t0, 4    # CK on pin 19
    srli x4, t0, 5    # Data out pin 18
    srli a4, t0, 6    # Data in pin 17
    li t2, 0x8c0000   # Output mask
    li x3, 1

    # Setup IO mask
    wriomsk t2
    setoe   t2
    wrevmsk x3
    wrio    t0  # CS high, clock and data low

outer_loop:
    # Wait for data length and addresses
.option rvc    
    rdf2 a3
    rdf2 t1
    rdf2 a2
    li x23, 0   # Set all pins low (clrio x0 = mv x23, x0 does not assemble compressed)
.option norvc

xfer_loop:
    # Read byte and shift to position
    lbu a0, 0(t1)
    slli a0, a0, 11

    # Clock low and set first bit
    and x21, a0, x4
    
.option rvc    
    li a5, 0

    addi a3, a3, -1
    slli a0, a0, 1

    setio x8 # Set clock bit, data and CS remain the same.
.option norvc    

.rept 7
    and x9, x21, a4 # Read bit and combine into a5
    or a5, a5, x9
    
    # Clock low and set next bit
    and x21, a0, x4

.option rvc    
    slli a5, a5, 1
    slli a0, a0, 1
.option norvc

    setio x8 # Set clock bit, data and CS remain the same.
.endr

    and x9, x21, a4 # Read bit
.option rvc    
    or a5, a5, x9  # Report read byte
    srli a5, a5, 17
    sb a5, 0(a2)
    addi t1, t1, 1
    addi a2, a2, 1

    bnez a3, xfer_loop

    wrio    t0  # CS high, clock and data low

    li a0, 1
    setev x3
    j outer_loop
