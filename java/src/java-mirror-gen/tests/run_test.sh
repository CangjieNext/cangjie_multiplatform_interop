#!/bin/sh
# Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
# This source file is part of the Cangjie project, licensed under Apache-2.0
# with Runtime Library Exception.
#
# See https://cangjie-lang.cn/pages/LICENSE for license information.

mkdir -p output
cd output
mkdir cj_mirror_gen_$1
cd cj_mirror_gen_$1
cjc --output-type=dylib ../../../../interoplib/javalib/Object.cj ../../../../interoplib/javalib/Array.cj ../../../../interoplib/javalib/String.cj
cjc -O2 --output-type=dylib ../../../../interoplib/jni.cj ../../../../interoplib/registry.cj

cjc ../../generated/cj_mirror_gen_$1/UNNAMED/src/*.cj -L./ --import-path=./ -ljava.lang -linteroplib.interop --output-type=dylib
