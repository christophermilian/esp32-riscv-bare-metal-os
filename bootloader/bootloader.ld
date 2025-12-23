/*
 * Linker script for bare-metal bootloader
 * Bootloader lives at 0x0 in flash (after partition table)
 */

MEMORY
{
    /* Bootloader uses a small section of IRAM/DRAM */
    iram_loader : ORIGIN = 0x403ce000, LENGTH = 0xE000   /* ~56KB for bootloader code */
    dram_loader : ORIGIN = 0x3FCD5800, LENGTH = 0x9800   /* ~38KB for bootloader data */

    /* Bootloader is loaded from flash offset 0x1000 */
    bootloader_flash : ORIGIN = 0x0, LENGTH = 0x8000     /* 32KB max bootloader size */
}

ENTRY(bootloader_entry)

SECTIONS
{
    .bootloader_text : ALIGN(4)
    {
        _bootloader_text_start = ABSOLUTE(.);
        *(.bootloader_text)
        *(.text*)
        _bootloader_text_end = ABSOLUTE(.);
    } > bootloader_flash

    .bootloader_rodata : ALIGN(4)
    {
        _bootloader_rodata_start = ABSOLUTE(.);
        *(.rodata*)
        _bootloader_rodata_end = ABSOLUTE(.);
    } > bootloader_flash

    .bootloader_data : ALIGN(4)
    {
        _bootloader_data_start = ABSOLUTE(.);
        *(.data*)
        _bootloader_data_end = ABSOLUTE(.);
    } > dram_loader AT > bootloader_flash

    .bootloader_bss (NOLOAD) : ALIGN(4)
    {
        _bss_start = ABSOLUTE(.);
        *(.bss*)
        *(COMMON)
        _bss_end = ABSOLUTE(.);
    } > dram_loader

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
    }
}
