#!/bin/bash
# create_disks.sh

mkdir -p disks

# FAT12
dd if=/dev/zero of=disks/fat12.img bs=1K count=1440
mkfs.fat -F 12 -n "FAT12DISK" disks/fat12.img

# FAT16
dd if=/dev/zero of=disks/fat16.img bs=1M count=32
mkfs.fat -F 16 -n "FAT16DISK" disks/fat16.img

# FAT32
for size in 64 128 256; do
    dd if=/dev/zero of=disks/fat32_${size}m.img bs=1M count=$size
    mkfs.fat -F 32 -n "FAT32_${size}M" disks/fat32_${size}m.img
done

for sec in 512 1024 2048 4096; do
    dd if=/dev/zero of=disks/fat32_s${sec}.img bs=1M count=64
    mkfs.fat -F 32 -S $sec -n "SEC${sec}" disks/fat32_s${sec}.img
done

echo "Done! Disks created in ./disks/"
ls -la disks/