/*
 * bio_blink.c - GPIO square wave driven by the BIO coprocessor
 *
 * Loads a simple toggle program onto BIO core 0 and generates
 * a 100 kHz square wave on PB13, verifiable with an oscilloscope.
 * The main CPU is free to do other work while the BIO runs.
 *
 * Wiring:
 *   PB13 (header pin 31) --> scope probe
 *   GND                 --> scope ground
 *   PB14                --> USB-serial RX
 */

#include "bao.h"
#include "hardware/bio.h"

/*
 * BIO program: toggles BIO pin 2 (PB2) with quantum timing.
 *
 *   addi  x5, x0, 4       ; bit mask for pin 2
 *   addi  x26, x5, 0      ; GPIO mask = pin 2
 *   addi  x24, x5, 0      ; pin 2 = output
 * loop:
 *   addi  x22, x5, 0      ; set pin 2 HIGH
 *   addi  x20, x0, 0      ; wait quantum
 *   addi  x23, x0, 0      ; clear pin 2 LOW
 *   addi  x20, x0, 0      ; wait quantum
 *   jal   x0, -16          ; loop
 */
static const uint32_t bio_blink_program[] = {
  0x00002537,
  0x000055b7,
  0xe2058593,
  0x00050d13,
  0x00050c13,
  0x00050b13,
  0x00058613,
  0x00000a13,
  0xfff60613,
  0xfe061ce3,
  0x00000b93,
  0x00058613,
  0x00000a13,
  0xfff60613,
  0xfe061ce3,
  0xfd9ff06f,
};

int main(void)
{
    bao_init();

    mini_printf("\r\nBIO Square Wave on PB2\r\n");

    bio_init(FCLK_HZ);
    bio_load_code_words(0, bio_blink_program, sizeof(bio_blink_program) / sizeof(bio_blink_program[0]));
    bio_map_pin(13);
    bio_set_divider(0, 17500, 0);  /* 40 kHz quantum (fclk / 17500) */
    bio_start_cores(0x1);

    mini_printf("BIO core 0 running. Expected: 100 kHz on PB2.\r\n");

    /* Main CPU is free */
    uint32_t count = 0;
    while (1) {
        if (!bio_core_trapped(0)) {
            mini_printf("  [%u] BIO running, PC=0x%03x\r\n",
                        count++, bio_core_pc(0));
        } else {
            mini_printf("  [%u] BIO TRAPPED!\r\n", count++);
        }
        delay_ms(2000);
    }
}
