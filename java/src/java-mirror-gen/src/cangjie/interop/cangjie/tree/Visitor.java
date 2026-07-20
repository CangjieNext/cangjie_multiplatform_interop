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

public interface Visitor {
    default void visit(CJTree.CompilationUnit tree) {
        visitDefault(tree);
    }

    default void visit(CJTree.Declaration.Annotation tree) {
        visitDefault(tree);
    }

    default void visit(CJTree.Declaration.Modifier tree) {
        visitDefault(tree);
    }

    default void visit(CJTree.Declaration.TypeDeclaration tree) {
        visitDefault(tree);
    }

    default void visit(CJTree.Declaration.FunctionDeclaration tree) {
        visitDefault(tree);
    }

    default void visit(CJTree.Declaration.VariableDeclaration.Parameter tree) {
        visitDefault(tree);
    }

    default void visit(CJTree.Declaration.VariableDeclaration.Variable tree) {
        visitDefault(tree);
    }

    default void visit(CJTree.GenericConstraint tree) {
        visitDefault(tree);
    }

    default void visit(CJTree.TypeArguments tree) {
        visitDefault(tree);
    }

    default void visit(CJTree.Expression.Literal.Numeric tree) {
        visitDefault(tree);
    }

    default void visit(CJTree.Expression.Literal.Rune tree) {
        visitDefault(tree);
    }

    default void visit(CJTree.Expression.Literal.String tree) {
        visitDefault(tree);
    }

    default void visit(CJTree.Expression.Literal.InterpolatedString tree) {
        visitDefault(tree);
    }

    default void visit(CJTree.Expression.Literal.Singleton tree) {
        visitDefault(tree);
    }

    default void visit(CJTree.Expression.Name.SimpleName.IdentifierName tree) {
        visitDefault(tree);
    }

    default void visit(CJTree.Expression.Name.SimpleName.OptionName tree) {
        visitDefault(tree);
    }

    default void visit(CJTree.Expression.Name.SimpleName.GenericName tree) {
        visitDefault(tree);
    }

    default void visit(CJTree.Expression.Name.SimpleName.FreeTypeVariableName tree) {
        visitDefault(tree);
    }

    default void visit(CJTree.Expression.Name.VariancedTypeName tree) {
        visitDefault(tree);
    }

    default void visit(CJTree.Expression.Name.QualifiedName tree) {
        visitDefault(tree);
    }

    default void visit(CJTree.Expression.Block tree) {
        visitDefault(tree);
    }

    void visitDefault(CJTree tree);
}
