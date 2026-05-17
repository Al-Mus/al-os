#!/bin/bash
# create_disks.sh
echo "Выбери тип FAT:"
echo "1) FAT12"
echo "2) FAT16"
echo "3) FAT32"
read -p "Ввод (1-3): " choice

case $choice in
    1)
        # FAT12
        dd if=/dev/zero of=disks/fat12.img bs=1K count=1440
        mkfs.fat -F 12 -n "FAT12" fat12.img
        ;;

    2)
        # FAT16
        dd if=/dev/zero of=fat16.img bs=1M count=32
        mkfs.fat -F 16 -n "FAT16" fat16.img
        ;;

    3)
        # FAT32
        dd if=/dev/zero of=fat32.img bs=1M count=268 status=progress
        mkfs.fat -F 32 -s 8 -n "FAT32" fat32.img
        ;;

    *)
        echo "Неверный ввод"
        exit 1
        ;;
esac

echo "Done! Disks created here"
ls -la *.img
