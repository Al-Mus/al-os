; Объявляем макросы, чтобы не писать 32 раза один и тот же код

; Макрос для прерываний, которые НЕ передают код ошибки (заталкиваем 0 сами)
%macro ISR_NOERRCODE 1
    global isr%1
    isr%1:
        cli                         ; Выключаем прерывания
        push byte 0                 ; Фиктивный код ошибки
        push byte %1                ; Номер прерывания
        jmp isr_common_stub
%endmacro

; Макрос для прерываний, которые автоматический передают код ошибки
%macro ISR_ERRCODE 1
    global isr%1
    isr%1:
        cli                         ; Выключаем прерывания (код ошибки уже в стеке)
        push byte %1                ; Номер прерывания
        jmp isr_common_stub
%endmacro

; Генерируем первые 32 обработчика исключений
ISR_NOERRCODE 0   ; 0: Divide By Zero
ISR_NOERRCODE 1   ; 1: Debug
ISR_NOERRCODE 2   ; 2: Non Maskable Interrupt
ISR_NOERRCODE 3   ; 3: Breakpoint
ISR_NOERRCODE 4   ; 4: Into Detected Overflow
ISR_NOERRCODE 5   ; 5: Out of Bounds
ISR_NOERRCODE 6   ; 6: Invalid Opcode
ISR_NOERRCODE 7   ; 7: No Coprocessor

ISR_ERRCODE   8   ; 8: Double Fault (с кодом ошибки!)
ISR_NOERRCODE 9   ; 9: Coprocessor Segment Overrun
ISR_ERRCODE   10  ; 10: Bad TSS
ISR_ERRCODE   11  ; 11: Segment Not Present
ISR_ERRCODE   12  ; 12: Stack Fault
ISR_ERRCODE   13  ; 13: General Protection Fault (с кодом ошибки!)
ISR_ERRCODE   14  ; 14: Page Fault (с кодом ошибки!)
ISR_NOERRCODE 15  ; 15: Unknown Interrupt

ISR_NOERRCODE 16  ; 16: Coprocessor Fault
ISR_ERRCODE   17  ; 17: Alignment Check
ISR_NOERRCODE 18  ; 18: Machine Check
ISR_NOERRCODE 19  ; 19: SIMD Floating-Point
ISR_NOERRCODE 20  ; 20: Virtualization Exception
ISR_ERRCODE   21  ; 21: Control Protection Exception
; 22-31 зарезервированы процессором, сделаем их пустышками без кода ошибки
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_NOERRCODE 30
ISR_NOERRCODE 31

; --- ОБЩИЙ ОБРАБОТЧИК ---
; Сюда прыгают все макросы выше. Он сохраняет состояние процессора
; и вызывает функцию на языке C.

extern isr_handler

isr_common_stub:
    ; Сохраняем регистры общего назначения
    pusha

    ; Сохраняем сегментные регистры данных
    mov ax, ds
    push eax

    ; Загружаем сегмент данных ядра (обычно 0x10 в GDT)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Вызываем наш C-обработчик.
    ; В стек уже положена вся структура регистров!
    call isr_handler

    ; Восстанавливаем оригинальные сегментные регистры
    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Восстанавливаем регистры общего назначения
    popa

    ; Очищаем стек от номера прерывания и кода ошибки (2 элемента по 4 байта = ESP + 8)
    add esp, 8

    ; Включаем прерывания обратно и возвращаемся из прерывания (команда iret!)
    sti
    iret


; Макрос для генерации аппаратных прерываний
; --- АППАРАТНЫЕ ПРЕРЫВАНИЯ (IRQ) ---

; Макрос для генерации аппаратных прерываний
%macro IRQ 2
global irq%1
irq%1:
    cli                 ; Отключаем прерывания на время обработки
    push byte 0         ; Фиктивный код ошибки для выравнивания стека
    push byte %2        ; Номер вектора (32, 33 и т.д.)
    jmp irq_common_stub ; Прыгаем в общий обработчик IRQ
%endmacro

; Генерируем обработчики для таймера и клавиатуры
IRQ 0, 32   ; irq0
IRQ 1, 33   ; irq1

extern irq_handler

irq_common_stub:
    pusha               ; Сохраняем регистры общего назначения (EAX, ECX и др.)

    mov ax, ds          ; Сохраняем оригинальный сегмент данных
    push eax

    mov ax, 0x10        ; Загружаем сегмент данных ядра из GDT
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Внимание: мы НЕ пушим esp. Си берет данные прямо из стека,
    ; точно так же, как это делает твой рабочий `isr_common_stub`!
    call irq_handler

    pop eax             ; Восстанавливаем оригинальный сегмент данных
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa                ; Восстанавливаем регистры общего назначения
    add esp, 8          ; Очищаем стек от номера прерывания и фиктивной ошибки

    sti                 ; Снова разрешаем прерывания
    iret                ; Безопасно возвращаемся в ядро
