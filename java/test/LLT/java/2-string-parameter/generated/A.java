// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

import static cangjie.lang.LibraryLoader.loadLibrary;

public class A extends M {
    static {
        loadLibrary("interoplib.interop.tests");
    }

    public static native int fieldInitStatic(int f);

    long self;

    A(int f) {
        super(fieldInitStatic(f));
        self = initCJObject();
    }

    @Override
    public void goo(String arg) {
        gooImpl(this.self, arg);
    }

    @Override
    public void gooMultipleArgs(String arg1, int arg2) {
        gooMultipleArgsImpl(this.self, arg1, arg2);
    }

    public static native void gooImpl(long self, String arg);

    public static native void gooMultipleArgsImpl(long self, String arg1, int arg2);

    public native long initCJObject();

    public static native void deleteCJObject(long self);

    @Override
    public void finalize() {
        deleteCJObject(this.self);
    }
}
