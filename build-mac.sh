set -e


nasm -f elf32 kernel.asm -o kasm.o

echo "Compiling toastOS"

x86_64-elf-gcc -m32 -c kernel.c -o kernel.o
x86_64-elf-gcc -m32 -c drivers/kio.c -o kio.o
x86_64-elf-gcc -m32 -c drivers/panic.c -o panic.o

echo "Linking"

x86_64-elf-ld -m elf_i386 -T link.ld -o kernel kasm.o kernel.o kio.o panic.o
# if this launched wwwwww

echo "Launching toastOS to emulate"
qemu-system-i386 -kernel kernel

echo "Cleaning up"

rm -rf *.o

echo "DONE!"
