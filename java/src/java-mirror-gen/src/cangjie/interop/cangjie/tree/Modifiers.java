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

package cangjie.interop.cangjie.tree;

public enum Modifiers {
    INVALID(""),
    SKIPPED(""),

    OVERRIDE("override"),

    PUBLIC("public"),
    PROTECTED("protected"),
    INTERNAL("internal"),
    PRIVATE("private"),

    STATIC("static"),
    OPEN("open"),
    ABSTRACT("abstract"),
    REDEF("redef"),

    CONST("const"),
    MUT("mut"),
    UNSAFE("unsafe"),
    REQUIRED("required"),
    FOREIGN("foreign"),

    OPERATOR("operator");

    public final String text;

    Modifiers(String text) {
        this.text = text;
    }

    public CJTree.Declaration.Modifier toCJTree() {
        return new CJTree.Declaration.Modifier(this);
    }

    @Override
    public String toString() {
        return this.text;
    }
}
