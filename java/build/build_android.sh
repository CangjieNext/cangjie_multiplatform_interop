#!/bin/bash
# Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
# This source file is part of the Cangjie project, licensed under Apache-2.0
# with Runtime Library Exception.
#
# See https://cangjie-lang.cn/pages/LICENSE for license information.

set -e
# export JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64
# Input parameters examples:
# $1: aarch64-linux-android31
# $2: <path/to/ndk>/toolchains/llvm/prebuilt/linux-x86_64/sysroot
# $3: <path/to/ndk>/toolchains/llvm/prebuilt/linux-x86_64/bin
# $4: linux_android31_aarch64_cjnative
# $5: aarch64-linux-android31

cd ../
mkdir -p dist

cd ./src/interoplib

clang -fstack-protector-strong -s -Wl,-z,relro,-z,now -fPIC -shared -o ../../dist/libcinteroplib.so -lcangjie-runtime -L${CANGJIE_HOME}/runtime/lib/$4 -I${JAVA_HOME}/include/ -I${JAVA_HOME}/include/linux c_core.c --target=$5 -isysroot $2

cjc --strip-all --link-options "-z relro" --link-options "-z now" --output-type=dylib --output-dir=../../dist --int-overflow wrapping jni.cj registry.cj -L../../dist -lcinteroplib --target=$1 --sysroot $2 -B $3

cjc --strip-all --link-options "-z relro" --link-options "-z now" --output-type=dylib --output-dir=../../dist --int-overflow wrapping --import-path=../../dist ./javalib/*.cj --target=$1 --sysroot $2 -B $3

javac -source 8 -target 8 -d ../../dist LibraryLoader.java
javac -source 8 -target 8 -d ../../dist \$\$NativeConstructorMarker.java
cd ../../dist
jar cf library-loader.jar cangjie/lang/LibraryLoader.class cangjie/lang/internal/\$\$NativeConstructorMarker.class

cd ../build
