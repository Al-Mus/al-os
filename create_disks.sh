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
dd if=/dev/zero of=fat32_min_4k.img bs=1M count=268 status=progress
mkfs.fat -F 32 -s 8 -n "FAT32-256M-4K" fat32_256m_4k.img

echo "Done! Disks created in ./disks/"
ls -la disks/
