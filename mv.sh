#!/bin/sh

IMG="fat32_min_4k.img"
MNT="mnt/disk"

echo "[*] Монтирую образ..."
sudo mount "$IMG" "$MNT" || exit 1

echo "[*] Копирую ELF файлы..."
sudo cp programs/*/*.elf "$MNT"/

echo "[*] Содержимое диска:"
ls -l "$MNT"

echo "[*] Размонтирую..."
sudo umount "$MNT"

echo "[*] Запуск QEMU..."
qemu-system-i386 -m 64M -boot d -cdrom AlSystem.iso -drive file="$IMG",format=raw
