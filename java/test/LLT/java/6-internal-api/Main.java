// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

import static cangjie.lang.LibraryLoader.loadLibrary;

public final class Main {
    static final String VALUE = "Lorem ipsum dolor sit amet";

    static {
        loadLibrary("interoplib.interop.tests");
    }

    public static void main(String[] args) {
        int result = foo();
        System.out.println(result);
    }

    private static native int foo();
}