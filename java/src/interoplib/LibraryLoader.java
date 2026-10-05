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
            String systemLibName = System.mapLibraryName(libName);
            nativeLoadCJLibrary(systemLibName, LibraryLoader.class.getClassLoader());
        } catch (LinkageError e) {
            // Handle UnsatisfiedLinkError caused by not found library with the libName.
            // When it is expected due to static linkage, for LTO applying
            // LibraryLoader.loadLibrary with proper libName should be added manually to customer's MainActivity.
        }
    }

    private static native void nativeLoadCJLibrary(String libName, ClassLoader classLoader);
}
