CC = gcc
ASM = nasm
LD = ld

CUR_DIR := $(CURDIR)

CFLAGS = -ffreestanding -O2 -Wall -Wextra -m32 -nostdlib -Ikernel
ASFLAGS = -f elf32

TARGET = kernel.elf
ISO = AL-OS.iso

comma := ,
DRIVE_ARG := $(if $(wildcard fat32.img),-drive file=fat32.img$(comma)format=raw$(comma)if=ide$(comma)index=1)
QEMU := qemu-system-i386 \
	-m 64M \
	-cdrom $(ISO) \
	$(DRIVE_ARG) \
	-boot d \
	-display gtk

# Находим ВСЕ файлы на Си
C_SRCS := $(shell find src/ -name '*.c')
C_OBJS := $(C_SRCS:.c=.o)

# Находим ВСЕ файлы на Ассемблере
ASM_SRCS := $(shell find src/ -name '*.asm')
ASM_OBJS := $(ASM_SRCS:.asm=.o)

# Все объектники
OBJS := $(ASM_OBJS) $(C_OBJS)

all: $(TARGET)

# Универсальное правило
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Правило для сборки ЛЮБОГО .asm файла в .o
%.o: %.asm
	$(ASM) $(ASFLAGS) $< -o $@

$(TARGET): $(OBJS)
	$(LD) -T linker.ld -m elf_i386 -nostdlib -o $(TARGET) $(OBJS)

iso: $(TARGET)
	mkdir -p iso/boot/grub
	cp $(TARGET) iso/boot/kernel.elf
	printf 'set timeout=1\nset default=0\nmenuentry "Boot Al-OS" {\n    multiboot /boot/kernel.elf\n    boot\n}\n' > iso/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) iso

iso_podman:
	podman run --rm -v "$(CURDIR):/build" mrleo0010/al-os-build sh -c "make iso"

iso_docker:
	docker run --rm -v "$(CURDIR):/build" mrleo0010/al-os-build sh -c "make iso"

clean-all:
	rm -f $(OBJS) $(TARGET) $(ISO)
	rm -rf iso

clean:
	rm -f $(OBJS) $(TARGET)
	rm -rf iso

run:
	$(QEMU)

run_net:
	$(QEMU) \
		-net nic,model=rtl8139 -net user

run_speaker:
	$(QEMU) \
		-audiodev pa,id=audio0 \
		-machine pcspk-audiodev=audio0

run_all:
	$(QEMU) \
		-net nic,model=rtl8139 -net user \
		-audiodev pa,id=audio0 \
		-machine pcspk-audiodev=audio0

run_debug:
	$(QEMU) \
	-s -S \
	-net nic,model=rtl8139 -net user \
	-audiodev pa,id=audio0 \
	-machine pcspk-audiodev=audio0

.PHONY: all iso clean clean-all run run_net run_speaker run_all run_debug iso_podman iso_docker
