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

; Загружает новую таблицу GDT
global gdt_flush

gdt_flush:
    mov eax, [esp + 4]  ; Получаем указатель на gdt_ptr из стека
    lgdt [eax]          ; Загружаем нашу новую таблицу GDT

    ; Перезагружаем сегмент кода. 0x08 — это смещение нашего сегмента кода в GDT.
    ; Этот прыжок очистит конвейер инструкций процессора и запишет 0x08 в регистр CS.
    jmp 0x08:.reload_segments

.reload_segments:
    ; Перезагружаем регистры сегментов данных.
    ; 0x10 — это смещение нашего сегмента данных в GDT.
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax          ; Перезагружаем сегмент стека
    ret

; Функция для IDT
global idt_flush

idt_flush:
    mov eax, [esp + 4]  ; eax теперь содержит адрес структуры idt_ptr (например, 0x105000)
    lidt [eax]          ; Процессор берет 6 байт ИЗ ПАМЯТИ по адресу в eax
    ret
