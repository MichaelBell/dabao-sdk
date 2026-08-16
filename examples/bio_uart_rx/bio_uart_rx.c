/*
 * bio_uart_rx.c - UART reception driven by the BIO coprocessor
 *
 * Loads a simple UART reception program onto BIO core 1.
 * The main CPU reads data through the FIFO.
 *
 * Wiring:
 *   PB2  --> UART RX
 */

#include "bao.h"
#include "hardware/bio.h"

static const uint32_t bio_uart_rx_program[] = {
#include "uart_rx.hex"
};

int main(void)
{
    bao_init();

    mini_printf("\r\nBIO UART RX on PB2\r\n");

    bio_init(FCLK_HZ);
    bio_load_code_words(1, bio_uart_rx_program, sizeof(bio_uart_rx_program) / sizeof(bio_uart_rx_program[0]));
    bio_map_pin(2);
    bio_set_divider(1, 6076/5, 0);  /* 115.2*5 kHz quantum (fclk / 6076) */
    bio_start_cores(0x2);

    mini_printf("BIO core 1 running.\r\n");

    /* Main CPU generates text */
    while (1) {
        if (bio_fifo_level(1) > 0) {
            char c = bio_fifo_pop(1);
            mini_printf("Received: '%c' (0x%02x)\r\n", c, (unsigned char)c);
        }
    }
}
