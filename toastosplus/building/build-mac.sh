#!/usr/bin/env bash
set -e

# =====================
# toastEngine Build System for toastOS++
# =====================

# Always run from the project root (one level up from building/)
cd "$(dirname "$0")/.."

export PATH=/usr/local/cross/bin:$PATH

ARCH=i386
CXX=x86_64-elf-g++
CC=x86_64-elf-gcc
LD=x86_64-elf-ld
ASM=nasm
QEMU=qemu-system-i386

# C++ flags for freestanding kernel
CXXFLAGS="-m32 -std=c++17 -ffreestanding -fno-stack-protector -fno-pic -mno-sse -mno-sse2 -mno-mmx -fno-exceptions -fno-rtti -I . -I drivers -nostdinc"
CFLAGS="-m32 -ffreestanding -fno-stack-protector -fno-pic -mno-sse -mno-sse2 -mno-mmx -I . -I drivers -nostdinc"
LDFLAGS="-m elf_i386 -T building/link.ld"

BUILT_DIR="built"
CACHE_FILE="$BUILT_DIR/build_cache.json"

# ---- Create built/ directory
mkdir -p "$BUILT_DIR"

# ---- Auto-detect kernel and service C++ files
# Exclude: string.cpp (conflicts with toast_libc), hello_ext.cpp (standalone app), settings.cpp (config app)
DRIVER_CPP_FILES=$(find drivers -maxdepth 1 -name "*.cpp" ! -name "string.cpp" | sed 's|.*/||' | sort)
SERVICES_CPP_FILES=$(find services -maxdepth 1 -name "*.cpp" | sed 's|.*/||' | sort)
# Exclude hello_ext.cpp from apps - it's a test app that loads separately
APPS_CPP_FILES=$(find apps -maxdepth 1 -name "*.cpp" ! -name "hello_ext.cpp" | sed 's|.*/||' | sort)

# Build object file lists (all in built/)
KERNEL_OBJS="$BUILT_DIR/kasm.o $BUILT_DIR/kernel.o $BUILT_DIR/setjmp.o"
DRIVER_OBJS=$(echo "$DRIVER_CPP_FILES" | sed "s|\.cpp$|.o|g" | sed "s|^|$BUILT_DIR/|")
SERVICES_OBJS=$(echo "$SERVICES_CPP_FILES" | sed "s|\.cpp$|.o|g" | sed "s|^|$BUILT_DIR/|")
APPS_OBJS=$(echo "$APPS_CPP_FILES" | sed "s|\.cpp$|.o|g" | sed "s|^|$BUILT_DIR/|")

OBJS="$KERNEL_OBJS $DRIVER_OBJS $SERVICES_OBJS $APPS_OBJS"

# ---- Load build cache (mtime per source file)
CACHE_LOOKUP_FILE="$BUILT_DIR/.cache_lookup.tmp"
if [ -f "$CACHE_FILE" ]; then
    python3 -c "
import json
with open('$CACHE_FILE') as f:
    c = json.load(f)
for k, v in c.items():
    print(k + '\t' + str(v.get('mtime', 0)))
" > "$CACHE_LOOKUP_FILE"
    echo "[*] Loaded build cache ($CACHE_FILE)"
else
    > "$CACHE_LOOKUP_FILE"
    echo "[*] No build cache found — performing full build"
fi

get_mtime() {
    stat -f "%m" "$1" 2>/dev/null || stat -c "%Y" "$1" 2>/dev/null || echo "0"
}

# Returns 0 (true) if src needs to be recompiled
needs_rebuild() {
    local src="$1"
    local obj="$2"
    local cur_mtime
    cur_mtime=$(get_mtime "$src")
    local cached
    cached=$(awk -F'\t' -v key="$src" '$1 == key {print $2; exit}' "$CACHE_LOOKUP_FILE")
    cached="${cached:-0}"
    [ ! -f "$obj" ] || [ "$cur_mtime" != "$cached" ]
}

# Track which sources were compiled this run
CACHE_TMP="$BUILT_DIR/.cache_updates.tmp"
> "$CACHE_TMP"  # truncate

mark_built() {
    local src="$1"
    printf '%s\t%s\n' "$src" "$(get_mtime "$src")" >> "$CACHE_TMP"
}

# Write updated cache JSON at end of build
save_cache() {
    python3 -c "
import json, sys

cache_file = sys.argv[1]
tmp_file   = sys.argv[2]

data = {}
try:
    with open(cache_file) as f:
        data = json.load(f)
except Exception:
    pass

with open(tmp_file) as f:
    for line in f:
        line = line.strip()
        if '\\t' in line:
            src, mtime = line.split('\\t', 1)
            data[src] = {'mtime': int(mtime)}

with open(cache_file, 'w') as f:
    json.dump(data, f, indent=2, sort_keys=True)
" "$CACHE_FILE" "$CACHE_TMP"
    rm -f "$CACHE_TMP"
}

# ---- WSL detection (optional, but nice)
if grep -qi microsoft /proc/version 2>/dev/null; then
    echo "[+] Running under WSL"
fi

# ---- Check required tools
for cmd in $ASM $CXX $LD $QEMU; do
    command -v $cmd >/dev/null 2>&1 || {
        echo "[!] $cmd is not installed. Aborting."
        exit 1
    }
done

RELINK=0

# ---- Assemble kernel.asm
if needs_rebuild "others/kernel.asm" "$BUILT_DIR/kasm.o"; then
    echo "[*] Assembling others/kernel.asm..."
    $ASM -f elf32 others/kernel.asm -o "$BUILT_DIR/kasm.o"
    mark_built "others/kernel.asm"
    RELINK=1
else
    echo "[=] Skipping others/kernel.asm (unchanged)"
fi

# ---- Assemble setjmp.asm
if needs_rebuild "others/setjmp.asm" "$BUILT_DIR/setjmp.o"; then
    echo "[*] Assembling others/setjmp.asm..."
    $ASM -f elf32 others/setjmp.asm -o "$BUILT_DIR/setjmp.o"
    mark_built "others/setjmp.asm"
    RELINK=1
else
    echo "[=] Skipping others/setjmp.asm (unchanged)"
fi

# ---- Compile kernel.cpp
if needs_rebuild "others/kernel.cpp" "$BUILT_DIR/kernel.o"; then
    echo "[*] Compiling others/kernel.cpp -> $BUILT_DIR/kernel.o"
    $CXX $CXXFLAGS -c others/kernel.cpp -o "$BUILT_DIR/kernel.o"
    mark_built "others/kernel.cpp"
    RELINK=1
else
    echo "[=] Skipping others/kernel.cpp (unchanged)"
fi

# ---- Compile driver C++ files
for file in $DRIVER_CPP_FILES; do
    src="drivers/$file"
    obj="$BUILT_DIR/${file%.cpp}.o"
    if needs_rebuild "$src" "$obj"; then
        echo "[*] Compiling $src -> $obj"
        $CXX $CXXFLAGS -c "$src" -o "$obj"
        mark_built "$src"
        RELINK=1
    else
        echo "[=] Skipping $src (unchanged)"
    fi
done

# ---- Compile service C++ files
for file in $SERVICES_CPP_FILES; do
    src="services/$file"
    obj="$BUILT_DIR/${file%.cpp}.o"
    if needs_rebuild "$src" "$obj"; then
        echo "[*] Compiling $src -> $obj"
        $CXX $CXXFLAGS -c "$src" -o "$obj"
        mark_built "$src"
        RELINK=1
    else
        echo "[=] Skipping $src (unchanged)"
    fi
done

# ---- Compile app C++ files
for file in $APPS_CPP_FILES; do
    src="apps/$file"
    obj="$BUILT_DIR/${file%.cpp}.o"
    if needs_rebuild "$src" "$obj"; then
        echo "[*] Compiling $src -> $obj"
        $CXX $CXXFLAGS -c "$src" -o "$obj"
        mark_built "$src"
        RELINK=1
    else
        echo "[=] Skipping $src (unchanged)"
    fi
done

# ---- Save updated build cache
if [ -s "$CACHE_TMP" ]; then
    save_cache
    echo "[*] Build cache updated ($CACHE_FILE)"
else
    rm -f "$CACHE_TMP"
fi

# ---- Link only if something changed
if [ $RELINK -eq 1 ]; then
    echo "[*] Linking kernel..."
    $LD $LDFLAGS -o kernel $OBJS
    echo "[*] Performing toastOS++ VM Prep"
    x86_64-elf-objcopy -O binary kernel kernel.bin
else
    echo "[=] Nothing changed — skipping link"
fi

# Create disk image if it doesn't exist
if [ ! -f toastos.img ]; then
    echo "[*] Creating 67MB disk image..."
    dd if=/dev/zero of=toastos.img bs=67M count=1 2>/dev/null
fi

echo "[+] toastOS++ VM Prep finished... can run in VM"

echo "[*] Launching toastOS++ in QEMU..."
qemu-system-i386 \
  -kernel kernel \
  -m 8M \
  -serial stdio \
  -d int \
    -drive file=toastos.img,format=raw,if=ide \
  -netdev user,id=n0 \
    -device rtl8139,netdev=n0

echo "[✓] toastOS++ run complete."
