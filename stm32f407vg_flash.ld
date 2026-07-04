/* STM32F407VGT6 Linker Script */
MEMORY
{
    FLASH  (rx) : ORIGIN = 0x08000000, LENGTH = 1024K
    RAM    (rwx): ORIGIN = 0x20000000, LENGTH = 128K
    CCMRAM (rw) : ORIGIN = 0x10000000, LENGTH = 64K
}

SECTIONS
{
    .text : {
        KEEP(*(.isr_vector))
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
        _etext = .;
    } > FLASH

    .data : {
        _sdata = .;
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > RAM AT > FLASH

    .bss : {
        _sbss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > RAM

    .heap (NOLOAD) : {
        _heap_start = .;
        . = . + 40K;
        _heap_end = .;
    } > RAM

    .stack (NOLOAD) : {
        _stack_start = .;
        . = . + 2K;
        _stack_end = .;
    } > RAM

    _estack = _stack_end;
}
