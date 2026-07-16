/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation. Huawei designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Huawei in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

package cangjie.interop;

import static vendor.com.sun.tools.javac.util.Assert.check;

import vendor.com.sun.tools.javac.code.Symbol;
import vendor.com.sun.tools.javac.util.Convert;
import vendor.com.sun.tools.javac.util.Name;

import java.util.ArrayList;
import java.util.Set;

public interface Utils {
    Set<String> ONLY_CANGJIE_KEYWORDS = Set.of(
            "as",
            "Bool",
            "extend",
            "Float16",
            "Float32",
            "Float64",
            "foreign",
            "from",
            "func",
            "in",
            "init",
            "inout",
            "Int16",
            "Int32",
            "Int64",
            "Int8",
            "IntNative",
            "is",
            "let",
            "var",
            "macro",
            "main",
            "match",
            "mut",
            "Nothing",
            "operator",
            "prop",
            "quote",
            "Rune",
            "spawn",
            "struct",
            "This",
            "type",
            "UInt16",
            "UInt32",
            "UInt64",
            "UInt8",
            "UIntNative",
            "Unit",
            "unsafe",
            "where",
            "with"
    );

    static boolean isToString(Symbol.MethodSymbol methodSymbol) {
        return methodSymbol.isPublic()
                && methodSymbol.getReturnType().tsym.toString().equals("java.lang.String")
                && methodSymbol.toString().equals("toString()");
    }

    static boolean isHashCode(Symbol.MethodSymbol methodSymbol) {
        return methodSymbol.isPublic()
                && methodSymbol.getReturnType().tsym.toString().equals("int")
                && methodSymbol.toString().equals("hashCode()");
    }

    static Name getFlatNameWithoutPackage(Symbol symbol) {
        return Convert.shortName(symbol.flatName());
    }

    private static boolean shouldMangle(int codePoint, boolean start) {
        // This method is an imperfect approximation of XID_Start/XID_Continue check.
        if (codePoint == '$') {
            return true;
        }
        if (start) {
            return !Character.isJavaIdentifierStart(codePoint);
        } else {
            return !Character.isJavaIdentifierPart(codePoint);
        }
    }

    private static String mangleIfNeeded(String name, char c) {
        check(!name.isEmpty());
        final var result = new StringBuilder();
        final var iterator = name.codePoints().iterator();
        var changed = false;
        while (iterator.hasNext()) {
            var codePoint = iterator.next();
            if (shouldMangle(codePoint, result.isEmpty())) {
                codePoint = (int) '_';
                changed = true;
            }
            result.appendCodePoint(codePoint);
        }
        String resStr = name;
        if (changed) {
            resStr = result.toString();
        }
        return ONLY_CANGJIE_KEYWORDS.contains(resStr) ? c + resStr + c : resStr;
    }

    static String addBackticksIfNeeded(String name) {
        return mangleIfNeeded(name, '`');
    }

    static String addBackticksIfNeeded(Name name) {
        return addBackticksIfNeeded(name.toString());
    }

    private static String addUnderscoresIfNeeded(String name) {
        return mangleIfNeeded(name, '_');
    }

    static String addUnderscoresIfNeeded(Name name) {
        return addUnderscoresIfNeeded(name.toString());
    }

    // Taken from java.lang.Integer internals
    static int stringLength(int x) {
        int d = 1;
        int tmp = x;
        if (x >= 0) {
            d = 0;
            tmp = -x;
        }
        int p = -10;
        for (int i = 1; i < 10; i++) {
            if (tmp > p) {
                return i + d;
            }
            p = 10 * p;
        }
        return 10 + d;
    }

    static String syntheticParameterName(int i, int sizeLength) {
        final String name;
        final var fmt = Integer.toString(i);
        name = "p" + "0".repeat(sizeLength - fmt.length()) + fmt;
        return name;
    }

    static String [] split(String s, int ch) {
        var off = 0;
        int next;
        var list = new ArrayList<String>();
        while ((next = s.indexOf(ch, off)) != -1) {
            list.add(s.substring(off, next));
            off = next + 1;
        }

        // No match
        if (off == 0) {
            return new String[]{s};
        }

        // Add remaining segment
        list.add(s.substring(off));

        // Construct result
        return list.toArray(new String[0]);
    }

    static int min(int length, int... items) {
        var min = length;
        for (final var item : items) {
            if (item == -1) {
                continue;
            }

            check(0 <= item && item < length);
            min = Math.min(min, item);
        }
        return min;
    }
}
