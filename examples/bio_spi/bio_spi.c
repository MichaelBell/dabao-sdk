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

#define FAST 1

static const uint32_t bio_spi_program[] = {
#if FAST
    #include "spi_fast.hex"
#else
    #include "spi.hex"
#endif
};

uint8_t rx_data[64] = { 0 };
void bio_spi_cmd(const uint8_t* cmd, uint32_t len) {
    bio_push_fifo2(len);  /* Number of bytes to transfer */
    for (uint32_t i = 0; i < len; i++) {
        while (BIO_SFR_FLEVEL & 0x80000) {
            /* Wait for FIFO space */
        }
        bio_push_fifo2(cmd[i]);

        /* Once 6 bytes exchanged start draining read FIFO */
        if (i >= 6) {
#if !FAST
            while (bio_fifo_empty(3)) {
                /* Wait for data */
            }
#endif
            rx_data[i-6] = bio_pop_fifo3();
        }
    }

    /* Finish reading data */
    for (uint32_t i = len >= 6 ? len - 6 : 0; i < len; i++) {
        while (bio_fifo_empty(3)) {
            /* Wait for data */
        }
        rx_data[i] = bio_pop_fifo3();
    }
}

int main(void)
{
    bao_init();

    trng_init();

    mini_printf("\r\nBIO SPI: CS on PC7, CK on PC3, CO on PC2, CI on PC1\r\n");

    bio_init(FCLK_HZ);
    bio_load_code_words(0, bio_spi_program, sizeof(bio_spi_program) / sizeof(bio_spi_program[0]));
    bio_map_pin(23);
    bio_map_pin(19);
    bio_map_pin(18);
    bio_map_pin(17);
    bio_set_divider(0, 40, 0);  /* 17.5 MHz quantum (fclk / 40) */
    bio_start_cores(0x1);  /* Start core 0 */

    mini_printf("BIO core running.\r\n");

    delay_ms(100);
    int cs = gpio_get(GPIO_PORT_C, 7);
    int clk = gpio_get(GPIO_PORT_C, 3);
    int d0 = gpio_get(GPIO_PORT_C, 2);
    mini_printf("Initial pin states: CS=%d, CK=%d, CO=%d\r\n", cs, clk, d0);

    const uint8_t id_cmd[12] = { 0x9F, 0x00, 0x00, 0x00 };  /* Read ID command */

    // Read ID from PSRAM
    bio_spi_cmd(id_cmd, sizeof(id_cmd));
    mini_printf("Read ID: %02x %02x %02x %02x ", rx_data[4], rx_data[5], rx_data[6], rx_data[7]);
    mini_printf("%02x %02x %02x %02x\r\n", rx_data[8], rx_data[9], rx_data[10], rx_data[11]);

    bool error = !(rx_data[4] == 0x0d && rx_data[5] == 0x5d && (rx_data[6] & 0xe0) == 0x40);
    mini_printf("PSRAM OK\r\n");

    uint32_t divider = 40;

    for (uint32_t count = 0; !error && count < divider; ++count) {
#if !FAST
        bio_set_divider(0, divider - count, 0);
#endif

        uint32_t random = trng_random();

        const uint32_t PSRAM_SIZE = 0x800000;
        const uint32_t WRITE_LEN = 0x10;

        uint64_t start_time = millis();
        uint8_t write_cmd[4 + 0x10] = { 0x02 };

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

        uint8_t read_cmd[4 + 0x10] = { 0x03 };
        for (uint32_t addr = 0; addr < PSRAM_SIZE && !error; addr += WRITE_LEN) {
            read_cmd[1] = addr >> 16;
            read_cmd[2] = (addr >> 8) & 0xFF;
            read_cmd[3] = addr & 0xFF;

            bio_spi_cmd(read_cmd, sizeof(read_cmd));

            for (uint32_t i = 0; i < WRITE_LEN; i += 2) {
                if (rx_data[4+i] != ((addr + i + random) & 0xFF)) {
                    mini_printf("Read error got %02x, expected %02x at addr %06x\r\n", rx_data[4+i], (addr + i + random) & 0xFF, addr+i);
                    error = true;
                }
                if (rx_data[5+i] != (((addr + i + random) >> 8) & 0xFF)) {
                    mini_printf("Read error got %02x, expected %02x at addr %06x\r\n", rx_data[5+i], ((addr + i + random) >> 8) & 0xFF, addr+i+1);
                    error = true;
                }
            }
        }

        uint64_t end_time = millis();

#if FAST
        mini_printf("Test loop %d completed in %u ms\r\n", count, (uint32_t)(end_time - start_time));
#else
        mini_printf("Test loop %d (divider %d) completed in %u ms\r\n", count, divider - count, (uint32_t)(end_time - start_time));
#endif
    }
}
