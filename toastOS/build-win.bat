@ECHO OFF
echo Buiding kernel main

wsl nasm -f elf32 kernel.asm -o kasm.o

echo Compiling c files

wsl gcc -m32 -fno-stack-protector -I drivers -c kernel.c -o kernel.o
wsl gcc -m32 -fno-stack-protector -I drivers -c drivers/kio.c -o kio.o
wsl gcc -m32 -fno-stack-protector -I drivers -c drivers/panic.c -o panic.o
wsl gcc -m32 -fno-stack-protector -I drivers -c drivers/file.c -o file.o
wsl gcc -m32 -fno-stack-protector -I drivers -c drivers/stdio.c -o stdio.o
wsl gcc -m32 -fno-stack-protector -I drivers -c drivers/string.c -o string.o
wsl gcc -m32 -fno-stack-protector -I drivers -c drivers/ata.c -o ata.o
wsl gcc -m32 -fno-stack-protector -I drivers -c drivers/fat16.c -o fat16.o
wsl gcc -m32 -fno-stack-protector -I drivers -c drivers/bootloader.c -o bootloader.o

echo Linking files

wsl ld -m elf_i386 -T link.ld -o kernel kasm.o kernel.o kio.o panic.o file.o stdio.o string.o ata.o fat16.o bootloader.o

echo Automatically emulating using qemu

wsl qemu-system-i386 -kernel kernel 

wsl rm -rf *.o
echo Cleaning up
echo GOOD!