@ECHO OFF
echo Buiding kernel main

wsl nasm -f elf32 kernel.asm -o kasm.o

echo Compiling c files

wsl gcc -m32 -fno-stack-protector -c kernel.c -o kernel.o
wsl gcc -m32 -fno-stack-protector -c drivers/kio.c -o kio.o
wsl gcc -m32 -fno-stack-protector -c drivers/funcs.c -o funcs.o

echo Linking files

wsl ld -m elf_i386 -T link.ld -o kernel kasm.o kernel.o kio.o funcs.o

echo Automatically emulating usingqemu

wsl qemu-system-i386 -kernel kernel 

wsl rm -rf *.o
echo Cleaning up
echo GOOD!