#!/bin/bash

CC=gcc   # потом заменишь на i686-elf-gcc

CFLAGS="-m32 -ffreestanding -nostdlib -nostartfiles -fno-pie -fno-stack-protector -O2 -Wall"
LDFLAGS="-no-pie -static -Wl,-Ttext=0x200000 -Wl,-e,_start"

build_dir() {
    dir=$1
    echo ""
    echo "[+] Entering: $dir"
    cd "$dir" || return

    for src in *.c; do
        [ -f "$src" ] || continue

        name=$(basename "$src" .c)
        echo "  -> Building $name.elf"

        $CC $CFLAGS $LDFLAGS -o "$name.elf" "$src"

        if [ $? -eq 0 ]; then
            size=$(stat -f%z "$name.elf" 2>/dev/null || stat -c%s "$name.elf")
            echo "     OK ($size bytes)"
        else
            echo "     FAILED"
        fi
    done

    cd - >/dev/null
}

for d in */; do
    build_dir "$d"
done

echo ""
echo "[✓] ALL BUILDS DONE"
