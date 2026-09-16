#include <stdint.h>
#include <hardware/regs/addressmap.h>

#define PAGE_VALID 0x1
#define PAGE_EXEC 0x8
#define PAGE_WRITE 0x4
#define PAGE_READ 0x2
#define PAGE_USER 0x10
#define PAGE_GLOBAL_MAPPING 0x20
#define PAGE_FLAGS_MASK 0x3F

__attribute__((section(".page_table")))
uint32_t page_table_root[4096];

#define NUM_L2_PAGE_TABLES 6
__attribute__((section(".page_table.entries")))
uint32_t page_table_l2[NUM_L2_PAGE_TABLES][4096];

extern void enable_page_table(uint32_t asid, uint32_t *root);

void set_l1_pte(uintptr_t va, uintptr_t pt_pa)
{
    uint32_t idx = (va >> 22);
    page_table_root[idx] = ((pt_pa & 0xFFFFF000) >> 2) | PAGE_VALID;
}

void set_l2_pte(uintptr_t va, uintptr_t pa, uint32_t l1_idx, uint32_t flags)
{
    uint32_t idx = (va >> 12) & 0x3FF;
    page_table_l2[l1_idx][idx] = ((pa & 0xFFFFF000) >> 2) | (flags & PAGE_FLAGS_MASK) | PAGE_VALID;
}

void setup_page_table(void)
{
    // Clear the page tables
    for (uint32_t i = 0; i < 4096; i++) {
        page_table_root[i] = 0;
    }
    for (uint32_t i = 0; i < NUM_L2_PAGE_TABLES; i++) {
        for (uint32_t j = 0; j < 4096; j++) {
            page_table_l2[i][j] = 0;
        }
    }

    // Set up L1 page table entries for the L2 page tables
    set_l1_pte(RRAM_BASE, (uintptr_t)page_table_l2[0]);
    set_l1_pte(SRAM_BASE, (uintptr_t)page_table_l2[1]);
    set_l1_pte(0x50000000UL, (uintptr_t)page_table_l2[2]);  // IFRAM, UDMA, BIO
    set_l1_pte(0x40000000UL, (uintptr_t)page_table_l2[3]);  // Peripherals
    set_l1_pte(0xE0000000UL, (uintptr_t)page_table_l2[4]);  // RV IRQs, etc
    set_l1_pte(0x58000000UL, (uintptr_t)page_table_l2[5]);  // SoC local peripherals

    // Set up mappings
    for (uintptr_t addr = RRAM_BASE; addr < RRAM_BASE + 0x39A000; addr += 0x1000) {
        set_l2_pte(addr, addr, 0, PAGE_READ | PAGE_EXEC | PAGE_USER | PAGE_GLOBAL_MAPPING);
    }
    for (uintptr_t addr = SRAM_BASE; addr < SRAM_BASE + 0x200000; addr += 0x1000) {
        uint32_t flags = PAGE_READ | PAGE_GLOBAL_MAPPING;

        // Only memory after page tables user accessible or writable.
        if (addr >= (uintptr_t)page_table_l2[NUM_L2_PAGE_TABLES]) {
            flags |= PAGE_WRITE | PAGE_USER;
        }

        set_l2_pte(addr, addr, 1, flags);
    }
    for (uintptr_t addr = 0x50000000UL; addr < 0x50130000UL; addr += 0x1000) {
        set_l2_pte(addr, addr, 2, PAGE_READ | PAGE_WRITE | PAGE_USER | PAGE_GLOBAL_MAPPING);
    }
    for (uintptr_t addr = 0x40000000UL; addr < 0x40100000UL; addr += 0x1000) {
        set_l2_pte(addr, addr, 3, PAGE_READ | PAGE_WRITE | PAGE_USER | PAGE_GLOBAL_MAPPING);
    }
    for (uintptr_t addr = 0xE0000000UL; addr < 0xE0020000UL; addr += 0x1000) {
        set_l2_pte(addr, addr, 4, PAGE_READ | PAGE_WRITE | PAGE_USER | PAGE_GLOBAL_MAPPING);
    }
    for (uintptr_t addr = 0x58000000UL; addr < 0x58020000UL; addr += 0x1000) {
        set_l2_pte(addr, addr, 5, PAGE_READ | PAGE_WRITE | PAGE_USER | PAGE_GLOBAL_MAPPING);
    }

    enable_page_table(1, page_table_root);
}
