#!/usr/bin/env bash
# build.sh for toastOS under WSL
# toastOS prioritizes macOS building
set -euo pipefail

# Check for required tools
for cmd in nasm x86_64-elf-gcc x86_64-elf-ld qemu-system-i386; do
    command -v $cmd >/dev/null 2>&1 || { echo "$cmd is not installed. Aborting."; exit 1; }
done

echo "Cleaning previous builds..."
rm -f *.o kernel

echo "Assembling kernel.asm..."
nasm -f elf32 kernel.asm -o kasm.o

echo "Compiling toastOS C sources..."
x86_64-elf-gcc -m32 -I drivers -c kernel.c -o kernel.o
x86_64-elf-gcc -m32 -I drivers -c drivers/kio.c -o kio.o
x86_64-elf-gcc -m32 -I drivers -c drivers/panic.c -o panic.o
x86_64-elf-gcc -m32 -I drivers -c drivers/file.c -o file.o
x86_64-elf-gcc -m32 -I drivers -c drivers/stdio.c -o stdio.o
x86_64-elf-gcc -m32 -I drivers -c drivers/string.c -o string.o
x86_64-elf-gcc -m32 -I drivers -c drivers/rtc.c -o rtc.o
x86_64-elf-gcc -m32 -I drivers -c drivers/registry.c -o registry.o
x86_64-elf-gcc -m32 -I drivers -c drivers/disk.c -o disk.o
x86_64-elf-gcc -m32 -I drivers -c drivers/editor.c -o editor.o
x86_64-elf-gcc -m32 -I drivers -c drivers/timer.c -o timer.o
x86_64-elf-gcc -m32 -I drivers -c drivers/toast_api.c -o toast_api.o
x86_64-elf-gcc -m32 -I drivers -c drivers/apps.c -o apps.o
x86_64-elf-gcc -m32 -I drivers -c drivers/network.c -o network.o
x86_64-elf-gcc -m32 -I drivers -c drivers/ethernet.c -o ethernet.o
x86_64-elf-gcc -m32 -I drivers -c drivers/arp.c -o arp.o
x86_64-elf-gcc -m32 -I drivers -c drivers/ip.c -o ip.o
x86_64-elf-gcc -m32 -I drivers -c drivers/icmp.c -o icmp.o
x86_64-elf-gcc -m32 -I drivers -c drivers/udp.c -o udp.o
x86_64-elf-gcc -m32 -I drivers -c drivers/netcmd.c -o netcmd.o

echo "Linking kernel..."
x86_64-elf-ld -m elf_i386 -T link.ld -o kernel kasm.o kernel.o kio.o panic.o file.o stdio.o string.o rtc.o registry.o disk.o editor.o timer.o toast_api.o apps.o network.o ethernet.o arp.o ip.o icmp.o udp.o netcmd.o

echo "Creating disk image..."
if [ ! -f toastOS.img ]; then
    dd if=/dev/zero of=toastOS.img bs=1M count=10
    echo "Created 10MB disk image"
fi

echo "Launching toastOS in QEMU..."
# WSL sometimes needs full path for QEMU
# Add RTL8139 network card for network stack testing
qemu-system-i386 -kernel kernel -m 512M -serial stdio -vga std -drive file=toastOS.img,format=raw,index=0,media=disk -netdev user,id=net0 -device rtl8139,netdev=net0

echo "Cleaning up object files..."
rm -f *.o

echo "toastOS run complete."
