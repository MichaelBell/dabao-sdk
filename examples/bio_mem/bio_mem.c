/*
 * bio_mem.c - Memory test driven by the BIO coprocessor
 *
 * Runs a simple memory test program on BIO core 0.
 */

#include "bao.h"
#include "hardware/bio.h"

static const uint32_t bio_mem_test_program[] = {
#include "mem.hex"
};

uint8_t rx_data[64] = { 0 };

int main(void)
{
    bao_init();

    mini_printf("\r\nBIO -> Main Memory Transfer Test\r\n");

    bio_init(FCLK_HZ);
    BIO_SFR_CONFIG |= (1 << 7);  // Enable direct BIO memory access
    bio_load_code_words(0, bio_mem_test_program, sizeof(bio_mem_test_program) / sizeof(bio_mem_test_program[0]));
    bio_map_pin(3);
    bio_start_cores(0x1);

    mini_printf("BIO core 0 running.\r\n");

    /* Main CPU generates text */
    while (1) {
        bio_push_fifo0((uintptr_t)rx_data);
        delay_ms(1000);

    // Flush CPU data cache
    __asm__ volatile (
        "fence\n"
        ".word 0x500F\n"
        "nop\n"
        "nop\n"
        "nop\n"
        "nop\n"
        ::: "memory"
    );        

        bool error = false;
        for (int i = 0; i < 64; i++) {
            mini_printf("%02x ", rx_data[i]);
            if (rx_data[i] != (((intptr_t)rx_data + i) & 0xFF)) {
                error = true;
            }
            if ((i & 0xF) == 0xF) {
                mini_printf("\r\n");
            }
        }
        if (error) {
            mini_printf("FAIL\r\n");
        } else {
            mini_printf("PASS\r\n");
        }
        break;
    }
}
