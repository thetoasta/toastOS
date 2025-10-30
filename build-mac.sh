set -e


nasm -f elf32 kernel.asm -o kasm.o


x86_64-elf-gcc -m32 -c kernel.c -o kc.o
x86_64-elf-gcc -m32 -c drivers/kio.c -o kio.o


x86_64-elf-ld -m elf_i386 -T link.ld -o kernel kasm.o kc.o kio.o

# if this launched wwwwww
qemu-system-i386 -kernel kernel
