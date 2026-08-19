.include "macros.s"

.section .text,"ax"

# SPI send and receive
# Pinout:
# - CS PC7 = 23 t0/s0
# - CK PC3 = 19 t1
# - CO PC2 = 18 a5 (Controller out)
# - CI PC1 = 17 a4 (Controller in)

# Data read from FIFO 2, written to FIFO 3.
# Format is length in bytes, then one entry per byte.

# Transfer rate is half the quantum clock

_start: 
    li t0, 0x800000   # CS on pin 23
    not s0, t0  # Invert CS
    srli t1, t0, 4    # CK on pin 19
    srli a5, t0, 5    # Data out pin 18
    srli a4, t0, 6    # Data in pin 17
    li t2, 0x8c0000   # Output mask

    # Setup IO mask
    wriomsk t2
    setoe   t2
    wrio    t0  # CS high, clock and data low

outer_loop:
    # Wait for data length
    rdf2 a3
    clrio s0  # Set CS low

xfer_loop:
    # Read byte from fifo2 and shift to position
    slli a0, x18, 11
    li a2, 8
    li x4, 0

bit_loop:
    # Clock low and set next bit
    and a1, a0, a5
    waitq
    wrio a1  # Isolated data bit, so this keeps clock and CS low.
    
    addi a2, a2, -1
    slli a0, a0, 1

    # Read bit
    and t2, x21, a4
    srli t2, t2, 17
    or x4, x4, t2

    waitq
    or x21, a1, t1 # Write OR of data bit and clock.  CS stays low.
    slli x4, x4, 1
    bnez a2, bit_loop

    srli x19, x4, 1 # Report read byte

    addi a3, a3, -1
    bnez a3, xfer_loop

    wrio    t0  # CS high, clock and data low
    j outer_loop
