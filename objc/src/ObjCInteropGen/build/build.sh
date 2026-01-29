#!/bin/bash
# Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
# This source file is part of the Cangjie project, licensed under Apache-2.0
# with Runtime Library Exception.
#
# See https://cangjie-lang.cn/pages/LICENSE for license information.

set -e
#sudo apt -y install clang libclang-dev

BACK=$(pwd)
BUILD="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
OUT=${BUILD}/output

cd ${BUILD}/..

cmake -B ${OUT}
cmake --build ${OUT}

cd ${BACK}
