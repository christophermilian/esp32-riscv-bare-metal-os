/*
 * Minimal bare-metal bootloader for ESP32-C3
 * This is the entry point after the ROM bootloader
 */

.section .bootloader_text, "ax"
.global bootloader_entry
.type bootloader_entry, @function

bootloader_entry:
    # Disable interrupts
    csrw mie, zero
    csrw mip, zero

    # Set up stack pointer
    # Use end of DRAM for bootloader stack
    lui sp, 0x3FCDF

    # Clear BSS section
    la a0, _bss_start
    la a1, _bss_end
clear_bss:
    beq a0, a1, bss_done
    sw zero, 0(a0)
    addi a0, a0, 4
    j clear_bss
bss_done:

    # Call bootloader main function
    call bootloader_main

    # If bootloader returns, load application
    # Application is at 0x42000020 (flash mapped address)
    li t0, 0x42000020
    jr t0

# Infinite loop fallback
hang:
    j hang

.size bootloader_entry, .-bootloader_entry
