#!/bin/bash
# Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
# This source file is part of the Cangjie project, licensed under Apache-2.0
# with Runtime Library Exception.
#
# See https://cangjie-lang.cn/pages/LICENSE for license information.

set -e

. ./run_test.sh simple
cd ../../
. ./run_test.sh str_param
cd ../../
. ./run_test.sh hashmap_param
cd ../../
