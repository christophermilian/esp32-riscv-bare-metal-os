/*
 * Application entry point
 * Called by bootloader
 */

.section .text.start
.global _start
.type _start, @function

_start:
    # Note: We can't use CSR instructions (csrw mie, csrw mip) here
    # because ESP-IDF bootloader starts us in User mode, not Machine mode
    # Interrupts are already disabled by the bootloader

    # NOTE: ESP-IDF bootloader has already done the following for us:
    # - Initialized .data section (copied from flash to RAM)
    # - Cleared .bss section
    # - Set up stack pointer
    # We don't need to redo this initialization!

    # Just call our C main function directly
    call main

    # If main returns, loop forever
hang:
    j hang

.size _start, .-_start
