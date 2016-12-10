#!/bin/bash

mkdir -p build

pushd build

cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release \
	-DLLVM_TARGETS_TO_BUILD="X86" \
	-DLLVM_INCLUDE_TOOLS=1 \
	-DLLVM_INCLUDE_TESTS=0 \
	-DLLVM_ENABLE_RTTI=1 \
	-DLLVM_ENABLE_ZLIB=0 \
	-DLLVM_PARALLEL_COMPILE_JOBS=8 -DLLVM_PARALLEL_LINK_JOBS=8 \
	--disable=curses \
	../llvm

make clean
make -j8

popd
