/*
 * bio_spi.c - SPI using the BIO coprocessor
 *
 * Wiring:
 * - CS PC7
 * - CK PC3
 * - CO PC2 (Controller out)
 * - CI PC1 (Controller in)
 */

#include "bao.h"
#include "hardware/bio.h"
#include "hardware/gpio.h"

#include "hardware/trng.h"

#define PSRAM_SIZE 0x800000
#define WRITE_LEN 0x20

static const uint32_t bio_spi_program[] = {
    #include "spi_mem.hex"
};

#define NANOPRINTF_IMPLEMENTATION
#include "nanoprintf.h"

static const uint32_t bio_uart_tx_program[] = {
#include "uart_tx.hex"
};

uint8_t rx_data[WRITE_LEN + 4] = { 0 };
void bio_spi_cmd(const uint8_t* cmd, uint32_t len) {
    // No need to flush cache here as the main CPU cache is write-through

    // Write command length, command pointer, and receive buffer pointer to BIO core
    bio_push_fifo2(len);
    bio_push_fifo2((uintptr_t)cmd);
    bio_push_fifo2((uintptr_t)rx_data);

    // Wait for completion
    while ((BIO_SFR_EVENT_STATUS & 0x1) == 0);
    BIO_SFR_EVENT_CLR = 0x1;

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
}

#if 1
void bio_printf(const char *fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    int len = npf_vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    for (int i = 0; i < len; i++) {
        while (bio_fifo_full(0)) {
            /* Wait for FIFO space */
        }
        bio_push_fifo0(buf[i]);
    }
}
#else
#define bio_printf mini_printf
#endif

int main(void)
{
    bao_init();

    trng_init();

    mini_printf("\r\nBIO SPI: CS on PC7, CK on PC3, CO on PC2, CI on PC1\r\n");

    bio_init(FCLK_HZ);
    BIO_SFR_CONFIG |= (1 << 7);  // Enable direct BIO memory access
    bio_load_code_words(0, bio_spi_program, sizeof(bio_spi_program) / sizeof(bio_spi_program[0]));
    bio_load_code_words(1, bio_uart_tx_program, sizeof(bio_uart_tx_program) / sizeof(bio_uart_tx_program[0]));
    bio_map_pin(23);
    bio_map_pin(19);
    bio_map_pin(18);
    bio_map_pin(17);
    bio_map_pin(3);
    bio_set_divider(1, 6076, 0);  /* 115.2 kHz quantum (fclk / 6076) */
    bio_start_cores(0x3);  /* Start cores 0 and 1 */

    mini_printf("BIO core running.\r\n");

    delay_ms(100);
    int cs = gpio_get(GPIO_PORT_C, 7);
    int clk = gpio_get(GPIO_PORT_C, 3);
    int d0 = gpio_get(GPIO_PORT_C, 2);
    bio_printf("Initial pin states: CS=%d, CK=%d, CO=%d\r\n", cs, clk, d0);

    const uint8_t id_cmd[12] = { 0x9F, 0x00, 0x00, 0x00 };  /* Read ID command */

    // Read ID from PSRAM
    bio_spi_cmd(id_cmd, sizeof(id_cmd));
    bio_printf("Read ID: %02x %02x %02x %02x ", rx_data[4], rx_data[5], rx_data[6], rx_data[7]);
    bio_printf("%02x %02x %02x %02x\r\n", rx_data[8], rx_data[9], rx_data[10], rx_data[11]);

    bool error = !(rx_data[4] == 0x0d && rx_data[5] == 0x5d && (rx_data[6] & 0xe0) == 0x40);
    bio_printf("PSRAM ");
    if (error) {
        bio_printf("not detected or ID mismatch\r\n");
    } else {
        bio_printf("OK\r\n");
    }
    error = false;

    for (uint32_t count = 0; !error; ++count) {
        uint32_t random = trng_random();

        uint64_t start_time = millis();
        uint8_t write_cmd[4 + WRITE_LEN] = { 0x02 };

        for (uint32_t addr = 0; addr < PSRAM_SIZE; addr += WRITE_LEN) {
            write_cmd[1] = addr >> 16;
            write_cmd[2] = (addr >> 8) & 0xFF;
            write_cmd[3] = addr & 0xFF;

            for (uint32_t i = 0; i < WRITE_LEN; i += 2) {
                write_cmd[4+i] = (addr + i + random) & 0xFF;
                write_cmd[5+i] = ((addr + i + random) >> 8) & 0xFF;
            }

            bio_spi_cmd(write_cmd, sizeof(write_cmd));
        }

        uint8_t read_cmd[4 + WRITE_LEN] = { 0x03 };
        for (uint32_t addr = 0; addr < PSRAM_SIZE && !error; addr += WRITE_LEN) {
            read_cmd[1] = addr >> 16;
            read_cmd[2] = (addr >> 8) & 0xFF;
            read_cmd[3] = addr & 0xFF;

            bio_spi_cmd(read_cmd, sizeof(read_cmd));

            for (uint32_t i = 0; i < WRITE_LEN; i += 2) {
                if (rx_data[4+i] != ((addr + i + random) & 0xFF)) {
                    bio_printf("Read error got %02x, expected %02x at addr %06x\r\n", rx_data[4+i], (addr + i + random) & 0xFF, addr+i);
                    error = true;
                }
                if (rx_data[5+i] != (((addr + i + random) >> 8) & 0xFF)) {
                    bio_printf("Read error got %02x, expected %02x at addr %06x\r\n", rx_data[5+i], ((addr + i + random) >> 8) & 0xFF, addr+i+1);
                    error = true;
                }
            }
        }

        uint64_t end_time = millis();

        bio_printf("Test loop %d completed in %u ms\r\n", count, (uint32_t)(end_time - start_time));
    }
}
