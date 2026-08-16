/*
 * bio_uart_loopback.c - UART loopback using the BIO coprocessor
 *
 * UART TX on BIO core 0, UART RX on BIO core 1
 * 
 * Wiring:
 *   PB3  --> UART TX
 *   PB2  --> UART RX
 */

#include "bao.h"
#include "hardware/bio.h"

static const uint32_t bio_uart_tx_program[] = {
#include "uart_tx.hex"
};

static const uint32_t bio_uart_rx_program[] = {
#include "uart_rx.hex"
};

int main(void)
{
    bao_init();

    mini_printf("\r\nBIO UART loopback: TX on PB3, RX on PB2\r\n");

    bio_init(FCLK_HZ);
    bio_load_code_words(0, bio_uart_tx_program, sizeof(bio_uart_tx_program) / sizeof(bio_uart_tx_program[0]));
    bio_map_pin(3);
    bio_set_divider(0, 6076, 0);  /* 115.2 kHz quantum (fclk / 6076) */

    bio_load_code_words(1, bio_uart_rx_program, sizeof(bio_uart_rx_program) / sizeof(bio_uart_rx_program[0]));
    bio_map_pin(2);
    bio_set_divider(1, 6076/5, 0);  /* 115.2*5 kHz quantum (fclk / 6076) */
    bio_start_cores(0x3);  /* Start both core 0 and core 1 */

    mini_printf("BIO cores running.\r\n");

    /* Main CPU does the loopback */
    while (1) {
        if (bio_fifo_level(1) > 0) {
            char c = bio_pop_fifo1();
            bio_push_fifo0(c);  /* Echo back to TX */
            //mini_printf("Received: '%c' (0x%02x)\r\n", c, (unsigned char)c);
        }
    }
}
