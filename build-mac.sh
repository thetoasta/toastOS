x86_64-elf-gcc -m32 -c boot/kernel.c -o kc.o
nasm -f elf32 boot/kernel.asm -o kasm.o
x86_64-elf-ld -m elf_i386 -T link.ld -o kernel kasm.o kc.o
