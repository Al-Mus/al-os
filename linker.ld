ENTRY(_start)

SECTIONS
{
    . = 1M;

    .multiboot ALIGN(4K) :
    {
        KEEP(*(.multiboot))
    }

    _text_start = .;
    .text ALIGN(4K) :
    {
        *(.text*)
    }
    _text_end = .;

    _rodata_start = .;
    .rodata ALIGN(4K) :
    {
        *(.rodata*)
    }
    _rodata_end = .;

    _data_start = .;
    .data ALIGN(4K) :
    {
        *(.data*)
    }
    _data_end = .;

    _bss_start = .;
    .bss ALIGN(4K) :
    {
        *(COMMON)
        *(.bss*)
    }
    _bss_end = .;

    end = .;

    /DISCARD/ :
    {
        *(.eh_frame)
        *(.note*)
    }
}
