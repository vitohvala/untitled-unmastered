#!/bin/bash


if [ ! -d "bin" ]; then
    echo "[CMD] : mkdir bin"
    mkdir bin
fi

if [ ! -d "linux_bin" ]; then
    echo "[CMD] : mkdir linux_bin"
    mkdir linux_bin
fi


if [ "$1" = "linux" ]; then
    CC='gcc'
    TARGET='src/linux_untitled.c'
    BIN='./linux_bin/untitled_linux'
    LIBS='-lX11 -lasound -lm'
    FLAGS='-Wall -Wextra -g'
    OPEN="./$BIN"
    
    echo "[compiling] : $CC -shared src/untitled.c -o linux_bin/untitled.s $FLAGS"
    $CC -shared src/untitled.c -o linux_bin/untitled.so $FLAGS
    echo "[compiling] : $CC $TARGET -o $BIN $LIBS $FLAGS"
    $CC $TARGET -o $BIN $LIBS $FLAGS

    if [ "$2" = "run" ]; then
        echo "[RUN] : $OPEN"
        $OPEN
    fi

elif [ 1 ]; then
    CC='x86_64-w64-mingw32-gcc' 
    TARGET='src/win32_untitled.c' 
    BIN='bin/untitled.exe'
    LIBS='-luser32 -lgdi32 -lm -lwinmm'
    OPEN="wine $BIN"
    FLAGS='-g -Wall -Wextra -Wno-cast-function-type -Wno-unused-variable -Werror'

    echo "[compiling] : $CC -shared  src/untitled.c -o bin/untitled.dll"
    $CC -shared  src/untitled.c -o bin/untitled.dll 
    echo "[compiling] : $CC $TARGET -o $BIN $LIBS $FLAGS"
    $CC $TARGET -o $BIN $LIBS $FLAGS 

    if [ "$1" = "run" ]; then
        echo "[RUN] : $OPEN"
        $OPEN
    fi
fi



