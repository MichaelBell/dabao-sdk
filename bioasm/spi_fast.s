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
    srli t1, t0, 4    # CK on pin 19
    srli a5, t0, 5    # Data out pin 18
    srli a4, t0, 6    # Data in pin 17
    li t2, 0x8c0000   # Output mask

    # Setup IO mask
    wriomsk t2
    setoe   t2
    wrio    t0  # CS high, clock and data low

outer_loop:
    # Wait for data length, shift to bits
    rdf2 a3
    clrio x0  # Set all pins low

xfer_loop:
    # Read byte from fifo2 and shift to position
    slli a0, x18, 11

    # Clock low and set first bit
    and a1, a0, a5
    wrio a1  # Isolated data bit, so this keeps/sets clock and CS low.
    
    li x4, 0
    addi a3, a3, -1
    slli a0, a0, 1

    or x21, a1, t1 # Write OR of data bit and clock.  CS stays low.

    and t2, x21, a4 # Read bit
    srli t2, t2, 17

.rept 6
    # Clock low and set next bit
    and a1, a0, a5
    wrio a1  # Isolated data bit, so this sets clock and CS low.
    
    or x4, x4, t2
    slli x4, x4, 1
    slli a0, a0, 1

    or x21, a1, t1 # Write OR of data bit and clock.  CS stays low.

    and t2, x21, a4 # Read bit
    srli t2, t2, 17
.endr

    # Clock low and set last bit
    and a1, a0, a5
    wrio a1  # Isolated data bit, so this sets clock and CS low.
    
    or x4, x4, t2
    slli x4, x4, 1
    slli a0, a0, 1

    or x21, a1, t1 # Write OR of data bit and clock.  CS stays low.

    and t2, x21, a4 # Read bit
    srli t2, t2, 17
    or x19, x4, t2  # Report read byte

    bnez a3, xfer_loop

    wrio    t0  # CS high, clock and data low
    j outer_loop
