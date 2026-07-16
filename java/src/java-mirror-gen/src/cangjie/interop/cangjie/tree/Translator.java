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

import static vendor.com.sun.tools.javac.util.Assert.checkNull;

public abstract class Translator<R> {
    private final Visitor visitor = new Visitor();

    protected R translate0(CJTree tree) {
        if (tree == null) {
            return null;
        }

        tree.accept(visitor);
        final var tmpResult = visitor.result;
        visitor.result = null;
        return tmpResult; // XXX cast
    }

    public abstract R translateDefault(CJTree tree);

    public R translate(CJTree.CompilationUnit tree) {
        return translateDefault(tree);
    }

    public R translate(CJTree.Declaration.Annotation tree) {
        return translateDefault(tree);
    }

    public R translate(CJTree.Declaration.Modifier tree) {
        return translateDefault(tree);
    }

    public R translate(CJTree.Declaration.TypeDeclaration tree) {
        return translateDefault(tree);
    }

    public R translate(CJTree.Declaration.FunctionDeclaration tree) {
        return translateDefault(tree);
    }

    public R translate(CJTree.Declaration.VariableDeclaration.Parameter tree) {
        return translateDefault(tree);
    }

    public R translate(CJTree.Declaration.VariableDeclaration.Variable tree) {
        return translateDefault(tree);
    }

    public R translate(CJTree.GenericConstraint tree) {
        return translateDefault(tree);
    }

    public R translate(CJTree.TypeArguments tree) {
        return translateDefault(tree);
    }

    public R translate(CJTree.Expression.Literal.Numeric tree) {
        return translateDefault(tree);
    }

    public R translate(CJTree.Expression.Literal.Rune tree) {
        return translateDefault(tree);
    }

    public R translate(CJTree.Expression.Literal.String tree) {
        return translateDefault(tree);
    }

    public R translate(CJTree.Expression.Literal.InterpolatedString tree) {
        return translateDefault(tree);
    }

    public R translate(CJTree.Expression.Literal.Singleton tree) {
        return translateDefault(tree);
    }

    public R translate(CJTree.Expression.Name.SimpleName.IdentifierName tree) {
        return translateDefault(tree);
    }

    public R translate(CJTree.Expression.Name.SimpleName.OptionName tree) {
        return translateDefault(tree);
    }

    public R translate(CJTree.Expression.Name.SimpleName.GenericName tree) {
        return translateDefault(tree);
    }

    public R translate(CJTree.Expression.Name.SimpleName.ErasedTypeVariableName tree) {
        return translateDefault(tree);
    }

    public R translate(CJTree.Expression.Name.VariancedTypeName tree) {
        return translateDefault(tree);
    }

    public R translate(CJTree.Expression.Name.QualifiedName tree) {
        return translateDefault(tree);
    }

    public R translate(CJTree.Expression.Block tree) {
        return translateDefault(tree);
    }

    private final class Visitor implements cangjie.interop.cangjie.tree.Visitor {
        private R result;

        @Override
        public void visit(CJTree.CompilationUnit tree) {
            checkNull(result);
            result = translate(tree);
        }

        @Override
        public void visit(CJTree.Declaration.Annotation tree) {
            checkNull(result);
            result = translate(tree);
        }

        @Override
        public void visit(CJTree.Declaration.Modifier tree) {
            checkNull(result);
            result = translate(tree);
        }

        @Override
        public void visit(CJTree.Declaration.TypeDeclaration tree) {
            checkNull(result);
            result = translate(tree);
        }

        @Override
        public void visit(CJTree.Declaration.FunctionDeclaration tree) {
            checkNull(result);
            result = translate(tree);
        }

        @Override
        public void visit(CJTree.Declaration.VariableDeclaration.Parameter tree) {
            checkNull(result);
            result = translate(tree);
        }

        @Override
        public void visit(CJTree.Declaration.VariableDeclaration.Variable tree) {
            checkNull(result);
            result = translate(tree);
        }

        @Override
        public void visit(CJTree.GenericConstraint tree) {
            checkNull(result);
            result = translate(tree);
        }

        @Override
        public void visit(CJTree.TypeArguments tree) {
            checkNull(result);
            result = translate(tree);
        }

        @Override
        public void visit(CJTree.Expression.Literal.Numeric tree) {
            checkNull(result);
            result = translate(tree);
        }

        @Override
        public void visit(CJTree.Expression.Literal.Rune tree) {
            checkNull(result);
            result = translate(tree);
        }

        @Override
        public void visit(CJTree.Expression.Literal.String tree) {
            checkNull(result);
            result = translate(tree);
        }

        @Override
        public void visit(CJTree.Expression.Literal.InterpolatedString tree) {
            checkNull(result);
            result = translate(tree);
        }

        @Override
        public void visit(CJTree.Expression.Literal.Singleton tree) {
            checkNull(result);
            result = translate(tree);
        }

        @Override
        public void visit(CJTree.Expression.Name.SimpleName.IdentifierName tree) {
            checkNull(result);
            result = translate(tree);
        }

        @Override
        public void visit(CJTree.Expression.Name.SimpleName.OptionName tree) {
            checkNull(result);
            result = translate(tree);
        }

        @Override
        public void visit(CJTree.Expression.Name.SimpleName.GenericName tree) {
            checkNull(result);
            result = translate(tree);
        }

        @Override
        public void visit(CJTree.Expression.Name.SimpleName.ErasedTypeVariableName tree) {
            checkNull(result);
            result = translate(tree);
        }

        @Override
        public void visit(CJTree.Expression.Name.VariancedTypeName tree) {
            checkNull(result);
            result = translate(tree);
        }

        @Override
        public void visit(CJTree.Expression.Name.QualifiedName tree) {
            checkNull(result);
            result = translate(tree);
        }

        @Override
        public void visit(CJTree.Expression.Block tree) {
            checkNull(result);
            result = translate(tree);
        }

        @Override
        public void visitDefault(CJTree tree) {
            checkNull(result);
            result = translateDefault(tree);
        }
    }
}
