#!/usr/bin/env bash
set -e

# =====================
# toastOS build script
# WSL compatible
# =====================

export PATH=/usr/local/cross/bin:$PATH

ARCH=i386
CC=x86_64-elf-gcc
LD=x86_64-elf-ld
ASM=nasm
QEMU=qemu-system-i386

CFLAGS="-m32 -ffreestanding -fno-stack-protector -fno-pic -I drivers"
FTCFLAGS="$CFLAGS -I freetype/include -DFT2_BUILD_LIBRARY -DFT_CONFIG_OPTION_NO_ASSEMBLER"
LDFLAGS="-m elf_i386 -T link.ld"

OBJS="kasm.o setjmp.o kernel.o toast_libc.o kio.o panic.o file.o stdio.o ata.o fat16.o bootloader.o time.o font_renderer.o"
FT_OBJS="ftsystem.o ftinit.o ftbase.o ftbitmap.o ftglyph.o ftdebug.o ftmm.o raster.o smooth.o truetype.o sfnt.o psnames.o"

# ---- WSL detection (optional, but nice)
if grep -qi microsoft /proc/version; then
    echo "[+] Running under WSL"
fi

# ---- Check required tools
for cmd in $ASM $CC $LD $QEMU; do
    command -v $cmd >/dev/null 2>&1 || {
        echo "[!] $cmd is not installed. Aborting."
        exit 1
    }
done

echo "[*] Cleaning previous builds..."
rm -f *.o kernel

echo "[*] Assembling kernel.asm..."
$ASM -f elf32 kernel.asm -o kasm.o
$ASM -f elf32 setjmp.asm -o setjmp.o

echo "[*] Compiling C sources..."
$CC $CFLAGS -I freetype/include -c kernel.c -o kernel.o
$CC $CFLAGS -c drivers/toast_libc.c -o toast_libc.o
$CC $CFLAGS -c drivers/kio.c -o kio.o
$CC $CFLAGS -c drivers/panic.c -o panic.o
$CC $CFLAGS -c drivers/file.c -o file.o
$CC $CFLAGS -c drivers/stdio.c -o stdio.o
$CC $CFLAGS -c drivers/ata.c -o ata.o
$CC $CFLAGS -c drivers/fat16.c -o fat16.o
$CC $CFLAGS -c drivers/bootloader.c -o bootloader.o
$CC $CFLAGS -c drivers/time.c -o time.o

echo "[*] Compiling FreeType font renderer..."
$CC $FTCFLAGS -c drivers/font_renderer.c -o font_renderer.o

echo "[*] Compiling FreeType library..."
$CC $FTCFLAGS -c freetype/src/base/ftsystem.c  -o ftsystem.o
$CC $FTCFLAGS -c freetype/src/base/ftinit.c    -o ftinit.o
$CC $FTCFLAGS -c freetype/src/base/ftbase.c    -o ftbase.o
$CC $FTCFLAGS -c freetype/src/base/ftbitmap.c  -o ftbitmap.o
$CC $FTCFLAGS -c freetype/src/base/ftglyph.c   -o ftglyph.o
$CC $FTCFLAGS -c freetype/src/base/ftdebug.c   -o ftdebug.o
$CC $FTCFLAGS -c freetype/src/base/ftmm.c      -o ftmm.o
$CC $FTCFLAGS -c freetype/src/raster/raster.c  -o raster.o
$CC $FTCFLAGS -c freetype/src/smooth/smooth.c  -o smooth.o
$CC $FTCFLAGS -c freetype/src/truetype/truetype.c -o truetype.o
$CC $FTCFLAGS -c freetype/src/sfnt/sfnt.c      -o sfnt.o
$CC $FTCFLAGS -c freetype/src/psnames/psnames.c -o psnames.o

echo "[*] Linking kernel..."
$LD $LDFLAGS -o kernel $OBJS $FT_OBJS

echo "[*] Performing toastOS VM Prep"
x86_64-elf-objcopy -O binary kernel kernel.bin

# Create disk image if it doesn't exist
if [ ! -f toastos.img ]; then
    echo "[*] Creating 64MB disk image..."
    dd if=/dev/zero of=toastos.img bs=1M count=64 2>/dev/null
fi

echo "[+] toastOS VM Prep finished... can run in VM"

echo "[*] Launching toastOS in QEMU..."
qemu-system-i386.exe \
  -kernel kernel \
  -m 20M \
  -serial stdio \
  -hda toastos.img \
  -no-reboot



echo "[*] Cleaning up object files..."
rm -f *.o

echo "[✓] toastOS run complete."
