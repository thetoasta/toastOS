
# build.sh for toastOS under WSL


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
x86_64-elf-gcc -m32 -I drivers -c drivers/ata.c -o ata.o
x86_64-elf-gcc -m32 -I drivers -c drivers/fat16.c -o fat16.o
x86_64-elf-gcc -m32 -I drivers -c drivers/bootloader.c -o bootloader.o

echo "Linking kernel..."
x86_64-elf-ld -m elf_i386 -T link.ld -o kernel kasm.o kernel.o kio.o panic.o file.o stdio.o string.o ata.o fat16.o bootloader.o

echo "Launching toastOS in QEMU..."
# WSL sometimes needs full path for QEMU
qemu-system-i386 -kernel kernel -m 512M -serial stdio

echo "Cleaning up object files..."
rm -f *.o

echo "toastOS run complete."
