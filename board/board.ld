/*
 * STM32F303xC memory setup.
 * to use with bootloader change the LDSCRIPT to board_with_bootloader.ld in the Makefile
 */
MEMORY
{
    flash : org = 0x08000000, len = 256K
    ram : org = 0x20000000, len = 40k
    ccmram : org = 0x10000000, len = 8k
}

INCLUDE ChibiOS/os/ports/GCC/ARMCMx/STM32F3xx/ld/STM32F303xC.ld
