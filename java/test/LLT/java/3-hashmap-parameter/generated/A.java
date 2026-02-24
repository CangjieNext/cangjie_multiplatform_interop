// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

import java.lang.Integer;
import java.util.HashMap;

import static cangjie.lang.LibraryLoader.loadLibrary;

public class A extends M {
    static {
        loadLibrary("interoplib.interop.tests");
    }

    long self;

    A(int arg) {
        super(arg);
        self = initCJObject(arg);
    }
    
    // need to ensure that generated internal init constructor not clashes with something
    private A(int arg, boolean init) {
        super(arg);
    }

    @Override
    public void goo(HashMap<Integer, String> arg) {
        gooImpl(this.self, arg);
    }

    public static native void gooImpl(long self, HashMap<Integer, String> arg);

    @Override
    public double boo(float size) {
        return booImpl(this.self, size);
    }

    public static native double booImpl(long self, float size);

    public native long initCJObject(int arg);

    public static native void deleteCJObject(long self);

    @Override
    public void finalize() {
        deleteCJObject(this.self);
    }
}
