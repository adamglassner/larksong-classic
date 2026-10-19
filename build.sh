#!/bin/sh
# Builds Larksong.exe with the MinGW-w64 32-bit toolchain.
# The flags keep it loadable on Windows 95: no C runtime, i486 instructions, subsystem 4.0.
set -e
cd "$(dirname "$0")"
V=$(tr -d ' \r\n' < VERSION)
MA=$(echo "$V" | cut -d. -f1); MI=$(echo "$V" | cut -d. -f2); PA=$(echo "$V" | cut -d. -f3)
mkdir -p build
cat > build/version.h <<H
#define VER_MAJOR $MA
#define VER_MINOR $MI
#define VER_PATCH $PA
#define VER_STRING "$V"
H
CC=${CC:-i686-w64-mingw32-gcc}
RC=${RC:-i686-w64-mingw32-windres}
$RC -I build -I src -O coff src/larksong.rc build/res.o
$CC -std=gnu99 -Os -march=i486 -mtune=generic -mno-sse -mno-mmx -mno-stack-arg-probe \
    -fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-builtin \
    -fno-tree-loop-distribute-patterns -ffunction-sections -fdata-sections \
    -Wall -Wno-unused-function -Wno-cast-function-type -I build -I src \
    src/core.c src/sound.c src/net.c src/larksong.c build/res.o \
    -nostdlib -Wl,-e,_entry -Wl,--subsystem,windows:4.0 \
    -Wl,--major-os-version,4 -Wl,--minor-os-version,0 \
    -Wl,--gc-sections -Wl,--no-insert-timestamp -s \
    -lgcc -lkernel32 -luser32 -lgdi32 -lwinmm -lshell32 -ladvapi32 -lcomdlg32 -lcomctl32 \
    -o build/Larksong.exe
ls -l build/Larksong.exe
