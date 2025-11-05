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

echo Linking files

wsl ld -m elf_i386 -T link.ld -o kernel kasm.o kernel.o kio.o panic.o file.o stdio.o string.o

echo Automatically emulating using qemu

wsl qemu-system-i386 -kernel kernel 

wsl rm -rf *.o
echo Cleaning up
echo GOOD!