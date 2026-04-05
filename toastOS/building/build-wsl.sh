#!/usr/bin/env bash
set -e

# =====================
# toastEngine Build System for toastOS
# =====================

# Always run from the project root (one level up from building/)
cd "$(dirname "$0")/.."

export PATH=/usr/local/cross/bin:$PATH

ARCH=i386
CC=x86_64-elf-gcc
LD=x86_64-elf-ld
ASM=nasm
QEMU=qemu-system-i386

CFLAGS="-m32 -ffreestanding -fno-stack-protector -fno-pic -mno-sse -mno-sse2 -mno-mmx -I . -I drivers -I freetype/include"
FTCFLAGS="$CFLAGS -DFT2_BUILD_LIBRARY -DFT_CONFIG_OPTION_NO_ASSEMBLER"
LDFLAGS="-m elf_i386 -T building/link.ld"

BUILT_DIR="built"
CACHE_FILE="$BUILT_DIR/build_cache.json"

# ---- Create built/ directory
mkdir -p "$BUILT_DIR"

# ---- Auto-detect kernel and service C files
# Exclude: string.c (conflicts with toast_libc), hello_ext.c (standalone app), settings.c (config app)
DRIVER_C_FILES=$(find drivers -maxdepth 1 -name "*.c" ! -name "font_renderer.c" ! -name "string.c" -printf "%f\n" | sort)
SERVICES_C_FILES=$(find services -maxdepth 1 -name "*.c" ! -printf "%f\n" | sort)
# Exclude hello_ext.c from apps - it's a test app that loads separately
APPS_C_FILES=$(find apps -maxdepth 1 -name "*.c" ! -name "hello_ext.c" -printf "%f\n" | sort)

# Build object file lists (all in built/)
KERNEL_OBJS="$BUILT_DIR/kasm.o $BUILT_DIR/kernel.o $BUILT_DIR/setjmp.o"
DRIVER_OBJS=$(echo "$DRIVER_C_FILES" | sed "s|\.c$|.o|g" | sed "s|^|$BUILT_DIR/|")
SERVICES_OBJS=$(echo "$SERVICES_C_FILES" | sed "s|\.c$|.o|g" | sed "s|^|$BUILT_DIR/|")
APPS_OBJS=$(echo "$APPS_C_FILES" | sed "s|\.c$|.o|g" | sed "s|^|$BUILT_DIR/|")

# Include font_renderer separately since it has special FreeType flags
OBJS="$KERNEL_OBJS $DRIVER_OBJS $SERVICES_OBJS $APPS_OBJS $BUILT_DIR/font_renderer.o"
FT_OBJS="$BUILT_DIR/ftsystem.o $BUILT_DIR/ftinit.o $BUILT_DIR/ftbase.o $BUILT_DIR/ftbitmap.o $BUILT_DIR/ftglyph.o $BUILT_DIR/ftdebug.o $BUILT_DIR/ftmm.o $BUILT_DIR/raster.o $BUILT_DIR/smooth.o $BUILT_DIR/truetype.o $BUILT_DIR/sfnt.o $BUILT_DIR/psnames.o"

# ---- Load build cache (mtime per source file)
declare -A CACHE_MTIMES
if [ -f "$CACHE_FILE" ]; then
    while IFS=$'\t' read -r file mtime; do
        CACHE_MTIMES["$file"]="$mtime"
    done < <(python3 -c "
import json
with open('$CACHE_FILE') as f:
    c = json.load(f)
for k, v in c.items():
    print(k + '\t' + str(v.get('mtime', 0)))
")
    echo "[*] Loaded build cache ($CACHE_FILE)"
else
    echo "[*] No build cache found — performing full build"
fi

get_mtime() {
    stat -c "%Y" "$1" 2>/dev/null || echo "0"
}

# Returns 0 (true) if src needs to be recompiled
needs_rebuild() {
    local src="$1"
    local obj="$2"
    local cur_mtime
    cur_mtime=$(get_mtime "$src")
    local cached="${CACHE_MTIMES[$src]:-0}"
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
if grep -qi microsoft /proc/version; then
    echo "[+] Running under WSL"
fi

# ---- Check required tools
for cmd in $ASM $CC $LD $QEMU; do
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

# ---- Compile kernel.c
if needs_rebuild "others/kernel.c" "$BUILT_DIR/kernel.o"; then
    echo "[*] Compiling others/kernel.c -> $BUILT_DIR/kernel.o"
    $CC $CFLAGS -I freetype/include -c others/kernel.c -o "$BUILT_DIR/kernel.o"
    mark_built "others/kernel.c"
    RELINK=1
else
    echo "[=] Skipping others/kernel.c (unchanged)"
fi

# ---- Compile driver C files
for file in $DRIVER_C_FILES; do
    src="drivers/$file"
    obj="$BUILT_DIR/${file%.c}.o"
    if needs_rebuild "$src" "$obj"; then
        echo "[*] Compiling $src -> $obj"
        $CC $CFLAGS -c "$src" -o "$obj"
        mark_built "$src"
        RELINK=1
    else
        echo "[=] Skipping $src (unchanged)"
    fi
done

# ---- Compile service C files
for file in $SERVICES_C_FILES; do
    src="services/$file"
    obj="$BUILT_DIR/${file%.c}.o"
    if needs_rebuild "$src" "$obj"; then
        echo "[*] Compiling $src -> $obj"
        $CC $CFLAGS -c "$src" -o "$obj"
        mark_built "$src"
        RELINK=1
    else
        echo "[=] Skipping $src (unchanged)"
    fi
done

# ---- Compile app C files
for file in $APPS_C_FILES; do
    src="apps/$file"
    obj="$BUILT_DIR/${file%.c}.o"
    if needs_rebuild "$src" "$obj"; then
        echo "[*] Compiling $src -> $obj"
        $CC $CFLAGS -c "$src" -o "$obj"
        mark_built "$src"
        RELINK=1
    else
        echo "[=] Skipping $src (unchanged)"
    fi
done

# ---- Compile FreeType font renderer
if needs_rebuild "drivers/font_renderer.c" "$BUILT_DIR/font_renderer.o"; then
    echo "[*] Compiling FreeType font renderer..."
    $CC $FTCFLAGS -c drivers/font_renderer.c -o "$BUILT_DIR/font_renderer.o"
    mark_built "drivers/font_renderer.c"
    RELINK=1
else
    echo "[=] Skipping drivers/font_renderer.c (unchanged)"
fi

# ---- Compile FreeType library
ft_sources=(
    "freetype/src/base/ftsystem.c:$BUILT_DIR/ftsystem.o"
    "freetype/src/base/ftinit.c:$BUILT_DIR/ftinit.o"
    "freetype/src/base/ftbase.c:$BUILT_DIR/ftbase.o"
    "freetype/src/base/ftbitmap.c:$BUILT_DIR/ftbitmap.o"
    "freetype/src/base/ftglyph.c:$BUILT_DIR/ftglyph.o"
    "freetype/src/base/ftdebug.c:$BUILT_DIR/ftdebug.o"
    "freetype/src/base/ftmm.c:$BUILT_DIR/ftmm.o"
    "freetype/src/raster/raster.c:$BUILT_DIR/raster.o"
    "freetype/src/smooth/smooth.c:$BUILT_DIR/smooth.o"
    "freetype/src/truetype/truetype.c:$BUILT_DIR/truetype.o"
    "freetype/src/sfnt/sfnt.c:$BUILT_DIR/sfnt.o"
    "freetype/src/psnames/psnames.c:$BUILT_DIR/psnames.o"
)
for entry in "${ft_sources[@]}"; do
    src="${entry%%:*}"
    obj="${entry##*:}"
    if needs_rebuild "$src" "$obj"; then
        echo "[*] Compiling $src -> $obj"
        $CC $FTCFLAGS -c "$src" -o "$obj"
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
    $LD $LDFLAGS -o kernel $OBJS $FT_OBJS
    echo "[*] Performing toastOS VM Prep"
    x86_64-elf-objcopy -O binary kernel kernel.bin
else
    echo "[=] Nothing changed — skipping link"
fi

# Create disk image if it doesn't exist
if [ ! -f toastos.img ]; then
    echo "[*] Creating 64MB disk image..."
    dd if=/dev/zero of=toastos.img bs=1M count=64 2>/dev/null
fi

echo "[+] toastOS VM Prep finished... can run in VM"

echo "[*] Launching toastOS in QEMU..."
qemu-system-i386.exe \
  -kernel kernel \
  -m 8M \
  -serial stdio \
    -drive file=toastos.img,format=raw,if=ide \
  -netdev user,id=n0 \
    -device rtl8139,netdev=n0

echo "[✓] toastOS run complete."
