#!/bin/bash

VERSION="release_39"

if [ ! -d "llvm" ]; then
	# install llvm
	svn co http://llvm.org/svn/llvm-project/llvm/branches/${VERSION} llvm

	# install clang
	pushd llvm/tools
	svn co http://llvm.org/svn/llvm-project/cfe/branches/${VERSION} clang
	popd

	# install compiler-rt and libcxx
	pushd llvm/projects
	svn co http://llvm.org/svn/llvm-project/compiler-rt/branches/${VERSION} compiler-rt
	svn co http://llvm.org/svn/llvm-project/libcxx/branches/${VERSION} libcxx
	svn co http://llvm.org/svn/llvm-project/libcxxabi/branches/${VERSION} libcxxabi
	popd
fi
