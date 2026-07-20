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

    /**
     * Base translator method for {@link CJTree.TypeArguments}
     *
     * @param tree tree instance to be translated
     * @return translation result
     */
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

    /**
     * Base translator method for {@link CJTree.Expression.Name.SimpleName.FreeTypeVariableName}
     *
     * @param tree tree instance to be translated
     * @return translation result
     */
    public R translate(CJTree.Expression.Name.SimpleName.FreeTypeVariableName tree) {
        return translateDefault(tree);
    }

    /**
     * Base translator method for {@link CJTree.Expression.Name.VariancedTypeName}
     *
     * @param tree tree instance to be translated
     * @return translation result
     */
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
            assert result == null;
            result = translate(tree);
        }

        @Override
        public void visit(CJTree.Declaration.Annotation tree) {
            assert result == null;
            result = translate(tree);
        }

        @Override
        public void visit(CJTree.Declaration.Modifier tree) {
            assert result == null;
            result = translate(tree);
        }

        @Override
        public void visit(CJTree.Declaration.TypeDeclaration tree) {
            assert result == null;
            result = translate(tree);
        }

        @Override
        public void visit(CJTree.Declaration.FunctionDeclaration tree) {
            assert result == null;
            result = translate(tree);
        }

        @Override
        public void visit(CJTree.Declaration.VariableDeclaration.Parameter tree) {
            assert result == null;
            result = translate(tree);
        }

        @Override
        public void visit(CJTree.Declaration.VariableDeclaration.Variable tree) {
            assert result == null;
            result = translate(tree);
        }

        @Override
        public void visit(CJTree.GenericConstraint tree) {
            assert result == null;
            result = translate(tree);
        }

        @Override
        public void visit(CJTree.TypeArguments tree) {
            assert result == null;
            result = translate(tree);
        }

        @Override
        public void visit(CJTree.Expression.Literal.Numeric tree) {
            assert result == null;
            result = translate(tree);
        }

        @Override
        public void visit(CJTree.Expression.Literal.Rune tree) {
            assert result == null;
            result = translate(tree);
        }

        @Override
        public void visit(CJTree.Expression.Literal.String tree) {
            assert result == null;
            result = translate(tree);
        }

        @Override
        public void visit(CJTree.Expression.Literal.InterpolatedString tree) {
            assert result == null;
            result = translate(tree);
        }

        @Override
        public void visit(CJTree.Expression.Literal.Singleton tree) {
            assert result == null;
            result = translate(tree);
        }

        @Override
        public void visit(CJTree.Expression.Name.SimpleName.IdentifierName tree) {
            assert result == null;
            result = translate(tree);
        }

        @Override
        public void visit(CJTree.Expression.Name.SimpleName.OptionName tree) {
            assert result == null;
            result = translate(tree);
        }

        @Override
        public void visit(CJTree.Expression.Name.SimpleName.GenericName tree) {
            assert result == null;
            result = translate(tree);
        }

        @Override
        public void visit(CJTree.Expression.Name.SimpleName.FreeTypeVariableName tree) {
            assert result == null;
            result = translate(tree);
        }

        @Override
        public void visit(CJTree.Expression.Name.VariancedTypeName tree) {
            assert result == null;
            result = translate(tree);
        }

        @Override
        public void visit(CJTree.Expression.Name.QualifiedName tree) {
            assert result == null;
            result = translate(tree);
        }

        @Override
        public void visit(CJTree.Expression.Block tree) {
            assert result == null;
            result = translate(tree);
        }

        @Override
        public void visitDefault(CJTree tree) {
            assert result == null;
            result = translateDefault(tree);
        }
    }
}
