// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package cangjie.lang;

import java.io.UnsupportedEncodingException;

public class LibraryLoader {
    public static void loadLibrary(String libName) {
        try {
            System.loadLibrary(libName);
        } catch (LinkageError e) {
            // Handle UnsatisfiedLinkError caused by not found library with the libName.
            // It can be expected due to static linkage, for LTO applying.
            // System.loadLibrary with proper libName should be added manually to CJMP customer's project MainActivity
            // Or LibraryLoader.loadLibrary for other Cangjie projects.
        }
        try {
            byte[] systemLibNameBytes = System.mapLibraryName(libName).getBytes("UTF-8");
            nativeLoadCJLibrary(systemLibNameBytes, LibraryLoader.class.getClassLoader());
        } catch (LinkageError e) {
            // Handle UnsatisfiedLinkError caused by not found java.internal library.
        } catch (UnsupportedEncodingException uee) {
            // UTF-8 is guaranteed to be supported for any major java implementation
        }
    }

    private static native void nativeLoadCJLibrary(byte[] libNameBytes, ClassLoader classLoader);
}
