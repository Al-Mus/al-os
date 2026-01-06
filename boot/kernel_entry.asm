; boot/kernel_entry.asm
[bits 32]
[extern kernel_main]

section .multiboot
    align 4
    dd 0x1BADB002
    dd 0x00
    dd -(0x1BADB002 + 0x00)

section .text
global _start
_start:
    call kernel_main
.hang:
    jmp .hang