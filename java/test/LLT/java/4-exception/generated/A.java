// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

import static cangjie.lang.LibraryLoader.loadLibrary;

public class A {
    static {
        loadLibrary("interoplib.interop.tests");
    }

    long self;

    A() {
        self = initCJObject();
    }

    public void goo() {
        gooImpl(this.self);
    }

    public void gooCJThrowGet() {
        gooCJThrowGetImpl(this.self);
    }

    public void gooCJThrowPut() throws Exception {
        gooCJThrowPutImpl(this.self);
    }

    public static native void gooImpl(long self);

    public static native void gooCJThrowPutImpl(long self);

    public static native void gooCJThrowGetImpl(long self);


    public native long initCJObject();

    public static native void deleteCJObject(long self);

    @Override
    public void finalize() {
        deleteCJObject(this.self);
    }
}