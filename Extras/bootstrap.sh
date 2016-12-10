#!/bin/bash

if [ ! -d "llvm" ]; then
	# install llvm
	svn co http://llvm.org/svn/llvm-project/llvm/trunk llvm

	# install clang
	pushd llvm/tools
	svn co http://llvm.org/svn/llvm-project/cfe/trunk clang
	popd

	# install compiler-rt and libcxx
	pushd llvm/projects
	svn co http://llvm.org/svn/llvm-project/compiler-rt/trunk compiler-rt
	svn co http://llvm.org/svn/llvm-project/libcxx/trunk libcxx
	svn co http://llvm.org/svn/llvm-project/libcxxabi/trunk libcxxabi
	popd
fi
