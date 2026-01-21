#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
LINKER_SCRIPT="$SCRIPT_DIR/program.ld"

if command -v i686-elf-gcc &> /dev/null; then
    CC="i686-elf-gcc"
    LD="i686-elf-ld"
    USE_CROSS=1
else
    CC="gcc"
    LD="ld"
    USE_CROSS=0
fi

CFLAGS="-m32 -ffreestanding -fno-pie -fno-stack-protector -O2 -Wall -Wextra -fno-builtin -c"

echo "========================================"
echo "     AL-OS Programs Build System"
echo "========================================"
echo "Compiler: $CC"
echo "Linker script: $LINKER_SCRIPT"
echo ""

build_program() {
    name=$1
    echo "[+] Building: $name"
    
    if [ ! -d "$name" ]; then
        echo "    Directory $name not found"
        return 1
    fi
    
    cd "$name"
    
    src_file=""
    if [ -f "${name}.c" ]; then
        src_file="${name}.c"
    elif [ -f "main.c" ]; then
        src_file="main.c"
    else
        for f in *.c; do
            if [ -f "$f" ]; then
                src_file="$f"
                break
            fi
        done
    fi
    
    if [ -z "$src_file" ]; then
        echo "    No source file found"
        cd ..
        return 1
    fi
    
    rm -f "${name}.o" "${name}.elf"
    
    echo "    Compiling $src_file..."
    $CC $CFLAGS "$src_file" -o "${name}.o" 2>&1
    if [ $? -ne 0 ]; then
        echo "    FAILED (compile)"
        rm -f "${name}.o"
        cd ..
        return 1
    fi
    
    echo "    Linking with script..."
    if [ $USE_CROSS -eq 1 ]; then
        $LD -T "$LINKER_SCRIPT" -o "${name}.elf" "${name}.o" 2>&1
    else
        $LD -m elf_i386 -T "$LINKER_SCRIPT" -o "${name}.elf" "${name}.o" 2>&1
    fi
    
    if [ $? -ne 0 ]; then
        echo "    FAILED (link)"
        rm -f "${name}.o"
        cd ..
        return 1
    fi
    
    rm -f "${name}.o"
    
    size=$(stat -f%z "${name}.elf" 2>/dev/null || stat -c%s "${name}.elf" 2>/dev/null)
    echo "    OK - ${name}.elf ($size bytes)"
    
    if command -v readelf &> /dev/null; then
        entry=$(readelf -h "${name}.elf" 2>/dev/null | grep "Entry point" | awk '{print $4}')
        load=$(readelf -l "${name}.elf" 2>/dev/null | grep "LOAD" | head -1 | awk '{print $3}')
        echo "    Entry: $entry, Load: $load"
    fi
    
    cd ..
    return 0
}

success=0
failed=0

if [ "$1" != "" ]; then
    build_program "$1"
    if [ $? -eq 0 ]; then
        success=1
    else
        failed=1
    fi
else
    for dir in */; do
        dirname="${dir%/}"
        if [ -d "$dirname" ]; then
            build_program "$dirname"
            if [ $? -eq 0 ]; then
                ((success++))
            else
                ((failed++))
            fi
            echo ""
        fi
    done
fi

echo "========================================"
echo "  Results: $success success, $failed failed"
echo "========================================"