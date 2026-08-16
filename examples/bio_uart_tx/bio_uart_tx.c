/*
 * bio_uart_tx.c - UART transmission driven by the BIO coprocessor
 *
 * Loads a simple UART transmission program onto BIO core 0.
 * The main CPU feeds data through the FIFO.
 *
 * Wiring:
 *   PB3  --> UART TX
 */

#include "bao.h"
#include "hardware/bio.h"

#define NANOPRINTF_IMPLEMENTATION
#include "nanoprintf.h"

static const uint32_t bio_uart_tx_program[] = {
  0x00300713,
  0x00100793,
  0x00e797b3,
  0x00078d13,
  0x00078c13,
  0x00078b13,
  0x00080513,
  0x00000a13,
  0x00000b93,
  0x00e51533,
  0x00800693,
  0x00000a13,
  0x00050a93,
  0x00155513,
  0xfff68693,
  0xfe0698e3,
  0x00000a13,
  0x00078b13,
  0x00000a13,
  0xfc9ff06f,
};

int main(void)
{
    bao_init();

    mini_printf("\r\nBIO UART TX on PB3\r\n");

    bio_init(FCLK_HZ);
    bio_load_code_words(0, bio_uart_tx_program, sizeof(bio_uart_tx_program) / sizeof(bio_uart_tx_program[0]));
    bio_map_pin(3);
    bio_set_divider(0, 6076, 0);  /* 115.2 kHz quantum (fclk / 6076) */
    bio_start_cores(0x1);

    mini_printf("BIO core 0 running.\r\n");

    /* Main CPU generates text */
    uint32_t count = 0;
    while (1) {
        char buf[64];
        int len = npf_snprintf(buf, sizeof(buf), "Hello from BIO UART TX! Count: %lu\r\n", count++);
        for (int i = 0; i < len; i++) {
            while (bio_fifo_full(0)) {
                /* Wait for FIFO space */
            }
            bio_push_fifo0(buf[i]);
        }
        delay_ms(1000);
    }
}
