// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package cangjie.lang;

public class LibraryLoader {
    public static void loadLibrary(String libName) {
        try {
            System.loadLibrary(libName);
            byte[] systemLibNameBytes = System.mapLibraryName(libName).getBytes("UTF-8");
            nativeLoadCJLibrary(systemLibNameBytes, LibraryLoader.class.getClassLoader());
        } catch (Exception ex) {}
    }

    private static native void nativeLoadCJLibrary(byte[] libNameBytes, ClassLoader classLoader);
}
