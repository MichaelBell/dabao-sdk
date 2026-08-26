.include "macros.s"

.section .text,"ax"

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
    mv a1, x16
    addi a0, a1, 64
1:
#    lbu x0, 0(a1)
    sb a1, 0(a1)
    addi a1, a1, 1
    bne a0, a1, 1b
    j _start
