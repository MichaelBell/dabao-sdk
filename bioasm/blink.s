.include "macros.s"

.section .text,"ax"

_start: 
   li  a0, 0x2000      # bit mask for pin 13
   li  a1, 20000       # Loop count
   wriomsk a0          # GPIO mask = output pin
   setoe   a0          # pin = output
1:
   setio   a0          # set pin HIGH
   mv  a2, a1
2:
   waitq               # wait quantum
   addi a2, a2, -1
   bnez a2, 2b

   clrio   x0          # clear pin LOW
   mv  a2, a1
3:
   waitq               # wait quantum
   addi a2, a2, -1
   bnez a2, 3b

   j   1b              # loop
