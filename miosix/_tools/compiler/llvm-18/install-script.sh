#!/usr/bin/env bash

# Script to build the llvm compiler required for Miosix.
# Usage: ./install-script
#
# Building Miosix is officially supported only through the llvm compiler built
# with this script. This is because this script patches the compiler.
# The kernel *won't* compile unless the correct compiler is used.
#
# This script will install miosix-llvm in /opt, creating links to
# binaries in /usr/bin.
# It should be run without root privileges, but it will ask for the root
# password when installing files to /opt and /usr/bin

#### Configuration tunables -- begin ####

# Uncomment if installing globally on this system
#PREFIX=/opt/miosix-llvm
#SUDO=sudo
# Uncomment if installing locally on this system, sudo isn't necessary
PREFIX=`pwd`/miosix-llvm
SUDO=

# Choose llvm build type
BUILD_TYPE=Release
#BUILD_TYPE=Debug

#### Configuration tunables -- end ####

quit() {
	echo $1
	exit 1
}

#
# Part 1, apply patches
#

PATCHES_DIR=`pwd`/patches
apply_patch() {
    local patch_file="$1"
    patch -p1 < "${PATCHES_DIR}/${patch_file}" || quit "Failed to apply patch ${patch_file}"
}

echo "Applying patches"
cd llvm-project
#apply_patch "Add-GCC-s-spare-dynamic-tags.patch"
#apply_patch "Implemented-single-pic-base.patch"
#apply_patch "libomp.patch"
echo "Successfully applied patches"

#
# Part 2: compile llvm
#

echo "Building llvm"
cd llvm
cmake -Bbuild -GNinja \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
    -DLLVM_ENABLE_PROJECTS="clang;lld" \
    -DLLVM_BUILD_LLVM_DYLIB=ON \
    -DLLVM_LINK_LLVM_DYLIB=ON \
    -DLLVM_OPTIMIZED_TABLEGEN=ON \
    -DLLVM_INCLUDE_EXAMPLES=OFF \
    -DLLVM_CCACHE_BUILD=1 \
    -DLLVM_PARALLEL_LINK_JOBS=1 || quit "Failed to configure llvm build"

cmake --build build --target install || quit "Failed to build and install llvm"
echo "Successfully built and installed llvm"
cd ..

# export the newly compiled compiler to use to compile libraries
export "PATH=${PREFIX}/bin:${PATH}"
# miosix clang toolchain file
TOOLCHAIN=`pwd`/../../../../cmake/Toolchains/clang.cmake

# TODO compile multilibs

#
# Part 3: compile libomp
#

# TODO crosscompile libomp also for other architectures
echo "Building libomp"
cd openmp
cmake -Bbuild -GNinja \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
    -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN}" \
    -DCMAKE_C_FLAGS="-mcpu=cortex-m0plus -mthumb" \
    -DCMAKE_CXX_FLAGS="-mcpu=cortex-m0plus -mthumb" \
    -DLIBOMP_ENABLE_SHARED=OFF \
    -DLLVM_CCACHE_BUILD=1 || quit "Failed to configure libomp build"

cmake --build build --target install || quit "Failed to build and install libomp"
echo "Successfully built and installed libomp"
cd ..

echo "Successfully installed miosix-llvm at ${PREFIX}"
