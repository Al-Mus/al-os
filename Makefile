CC = gcc
ASM = nasm
LD = ld

CFLAGS = -ffreestanding -O2 -Wall -Wextra -m32 -nostdlib -Ikernel
ASFLAGS = -f elf32

TARGET = kernel.elf
ISO = AlSystem.iso

OBJS = boot/kernel_entry.o kernel/kernel.o \
       kernel/keyboard.o \
       kernel/fs.o \
       kernel/vga.o kernel/string.o

all: $(TARGET)

boot/kernel_entry.o: boot/kernel_entry.asm
	$(ASM) $(ASFLAGS) $< -o $@

kernel/%.o: kernel/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(LD) -T linker.ld -m elf_i386 -o $(TARGET) $(OBJS)

iso: $(TARGET)
	mkdir -p iso/boot/grub
	cp $(TARGET) iso/boot/kernel.elf
	printf "set timeout=1\nset default=0\nmenuentry \"Test Al System\" {\n    multiboot /boot/kernel.elf\n    boot\n}\n" > iso/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) iso

clean:
	rm -f $(OBJS) $(TARGET) $(ISO)
	rm -rf iso