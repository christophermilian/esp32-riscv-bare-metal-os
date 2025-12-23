/*
 * Bare-metal linker script fragment for ESP32-C3
 *
 * This is a FRAGMENT that works with ESP-IDF's linker scripts.
 * ESP-IDF's memory.ld and sections.ld are included before this.
 *
 * Boot sequence:
 * 1. ROM bootloader (in chip)
 * 2. ESP-IDF 2nd stage bootloader (flash 0x0000)
 * 3. Partition table (flash 0x8000)
 * 4. Our bare-metal app (flash 0x10000) <- ESP-IDF loads and jumps to _start
 */

/* Override entry point - make _start the entry instead of call_start_cpu0 */
ENTRY(_start)

SECTIONS
{
    /*
     * Prepend our _start to the beginning of the text section in flash
     * This ensures _start is the first code to run after bootloader
     */
    .flash.text :
    {
        . = ALIGN(4);
        _bare_metal_start = .;
        *(.text.start)    /* Our boot.S _start function */
        . = ALIGN(4);
    }

    /*
     * Define custom symbols needed by boot.S
     * ESP-IDF already defines _data_start, _data_end, _bss_start, _bss_end
     * We just need to add _data_load and _stack_top
     */
    .dram0.data :
    {
        /* Mark where .data is stored in flash (for copying to RAM) */
        _data_load = LOADADDR(.dram0.data);
    }

    /* Stack: ESP-IDF manages this, but we define _stack_top for our use */
    /* Place stack at end of available DRAM */
    _stack_top = ORIGIN(dram0_0_seg) + LENGTH(dram0_0_seg);
}
