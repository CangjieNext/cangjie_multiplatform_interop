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

import java.util.List;

public abstract class Scanner implements Visitor {
    protected void scan(CJTree tree) {
        if (tree != null) {
            tree.accept(this);
        }
    }

    protected final void scan(List<? extends CJTree> trees) {
        if (trees != null) {
            trees.forEach(this::scan);
        }
    }

    @Override
    public void visit(CJTree.CompilationUnit tree) {
        scan(tree.getPackageName());
        scan(tree.types);
    }

    @Override
    public void visit(CJTree.Declaration.Annotation tree) {
        scan(tree.expression);
        scan(tree.argumentList);
    }

    @Override
    public void visit(CJTree.Declaration.Modifier tree) {
    }

    @Override
    public void visit(CJTree.Declaration.TypeDeclaration tree) {
        scan(tree.annotations);
        scan(tree.getType());
        scan(tree.typeParameters());
        scan(tree.supers);
        scan(tree.constraints());
        scan(tree.declarations);
    }

    @Override
    public void visit(CJTree.Declaration.FunctionDeclaration tree) {
        scan(tree.annotations);
        scan(tree.typeParameters());
        scan(tree.valueParameters);
        scan(tree.getReturnType());
        scan(tree.constraints());
        scan(tree.getBody());
    }

    @Override
    public void visit(CJTree.Declaration.VariableDeclaration.Parameter tree) {
        scan(tree.annotations);
        scan(tree.getType());
        scan(tree.getInitializer());
    }

    @Override
    public void visit(CJTree.Declaration.VariableDeclaration.Variable tree) {
        scan(tree.annotations);
        scan(tree.getType());
        scan(tree.getInitializer());
    }

    @Override
    public void visit(CJTree.GenericConstraint tree) {
        scan(tree.variable);
        scan(tree.bounds);
    }

    @Override
    public void visit(CJTree.TypeArguments tree) {
        scan(tree.arguments);
    }

    @Override
    public void visit(CJTree.Expression.Literal.Numeric tree) {
    }

    @Override
    public void visit(CJTree.Expression.Literal.Rune tree) {
    }

    @Override
    public void visit(CJTree.Expression.Literal.String tree) {
    }

    @Override
    public void visit(CJTree.Expression.Literal.InterpolatedString tree) {
        scan(tree.parts);
    }

    @Override
    public void visit(CJTree.Expression.Literal.Singleton tree) {
    }

    @Override
    public void visit(CJTree.Expression.Name.SimpleName.IdentifierName tree) {
    }

    @Override
    public void visit(CJTree.Expression.Name.SimpleName.OptionName tree) {
        scan(tree.getName());
    }

    @Override
    public void visit(CJTree.Expression.Name.SimpleName.GenericName tree) {
        scan(tree.arguments);
    }

    @Override
    public void visit(CJTree.Expression.Name.SimpleName.FreeTypeVariableName tree) {
    }

    @Override
    public void visit(CJTree.Expression.Name.VariancedTypeName tree) {
        scan(tree.typeName);
    }

    @Override
    public void visit(CJTree.Expression.Name.QualifiedName tree) {
        scan(tree.getLeft());
        scan(tree.getRight());
    }

    @Override
    public void visit(CJTree.Expression.Block tree) {
        scan(tree.expressions);
    }

    @Override
    public final void visitDefault(CJTree tree) {
        throw new RuntimeException("Not implemented: " + tree.getClass().getName());
    }
}
