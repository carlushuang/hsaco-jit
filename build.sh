#!/bin/sh
LLVM_DIR=/usr/lib/llvm-14/
CC=/usr/bin/clang++-14
CXXFLAGS=`$LLVM_DIR/bin/llvm-config --cxxflags`
#LD_LIBS="$LLVM_DIR/lib/libLLVM*.a"
LD_LIBS="`$LLVM_DIR/bin/llvm-config --ldflags` `$LLVM_DIR/bin/llvm-config --libs  --link-static`"
LDFLAGS="$LD_LIBS -pthread -ldl -lz -ltinfo"

$CC $CXXFLAGS  -O3 hsaco_jit.cpp $LDFLAGS  -o hsaco_jit.exe
