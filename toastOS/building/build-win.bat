@ECHO OFF
echo Buiding kernel main

REM Run from the project root (one level up from building/)
cd /d "%~dp0\.."

wsl nasm -f elf32 others/kernel.asm -o kasm.o

echo Compiling c files

wsl gcc -m32 -fno-stack-protector -I . -I drivers -c others/kernel.c -o kernel.o
wsl gcc -m32 -fno-stack-protector -I . -I drivers -c drivers/kio.c -o kio.o
wsl gcc -m32 -fno-stack-protector -I . -I drivers -c drivers/panic.c -o panic.o
wsl gcc -m32 -fno-stack-protector -I . -I drivers -c drivers/file.c -o file.o
wsl gcc -m32 -fno-stack-protector -I . -I drivers -c drivers/stdio.c -o stdio.o
wsl gcc -m32 -fno-stack-protector -I . -I drivers -c drivers/string.c -o string.o
wsl gcc -m32 -fno-stack-protector -I . -I drivers -c drivers/ata.c -o ata.o
wsl gcc -m32 -fno-stack-protector -I . -I drivers -c drivers/fat16.c -o fat16.o
wsl gcc -m32 -fno-stack-protector -I . -I drivers -c drivers/bootloader.c -o bootloader.o
wsl gcc -m32 -fno-stack-protector -I . -I drivers -c drivers/mmu.c -o mmu.o
wsl gcc -m32 -fno-stack-protector -I . -I drivers -c drivers/toastcc.c -o toastcc.o
wsl gcc -m32 -fno-stack-protector -I . -I drivers -c drivers/net.c -o net.o

echo Linking files

wsl ld -m elf_i386 -T building/link.ld -o kernel kasm.o kernel.o kio.o panic.o file.o stdio.o string.o ata.o fat16.o bootloader.o mmu.o toastcc.o net.o

echo Automatically emulating using qemu

wsl qemu-system-i386 -kernel kernel 

wsl rm -rf *.o
echo Cleaning up
echo GOOD!