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

import static vendor.com.sun.tools.javac.util.Assert.check;

import cangjie.interop.Utils;
import cangjie.interop.cangjie.QualifiedName;
import cangjie.interop.cangjie.emit.CangjieEmitVisitor;
import cangjie.interop.cangjie.emit.IndentedString;
import cangjie.interop.cangjie.sema.TypeKind;

import java.util.ArrayList;
import java.util.Collection;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Objects;
import java.util.Set;

public abstract sealed class CJTree {
    private static final CangjieEmitVisitor CANGJIE_EMIT_VISITOR = new CangjieEmitVisitor();
    private static final Scanner COMMENT_OUT_GENERICS_VISITOR = new Scanner() {
        @Override
        public void visit(TypeArguments tree) {
            tree.commentOut();
        }

        @Override
        public void visit(GenericConstraint tree) {
        }

        @Override
        public void visit(Expression.Name.SimpleName.ErasedTypeVariableName tree) {
            tree.commentOut();
            scan(tree.erasedTypeName);
        }

        @Override
        public void visit(Expression.Name.SimpleName.GenericName tree) {
            if (Objects.equals(tree.identifier, "JArray")) {
                scan(tree.arguments.arguments);
            } else {
                super.visit(tree);
            }
        }
    };

    public abstract void accept(Visitor visitor);

    public final IndentedString produce() {
        COMMENT_OUT_GENERICS_VISITOR.scan(this);
        return CANGJIE_EMIT_VISITOR.emit(this);
    }

    @Override
    public String toString() {
        return produce().text();
    }

    public static final class CompilationUnit extends CJTree {
        public final Set<QualifiedName> imports = new LinkedHashSet<>();
        public final Set<QualifiedName> wildcardImports = new LinkedHashSet<>();
        public final List<CJTree> types = new ArrayList<>();
        private Expression.Name packageName;

        public Expression.Name getPackageName() {
            return packageName;
        }

        public void setPackageName(Expression.Name packageName) {
            this.packageName = packageName;
        }

        @Override
        public void accept(Visitor visitor) {
            visitor.visit(this);
        }
    }

    public abstract static sealed class Declaration extends CJTree {
        public final List<Annotation> annotations = new ArrayList<>();
        public final List<Modifier> modifiers = new ArrayList<>();

        public static final class Annotation extends CJTree {
            public final Expression expression;
            public final List<Expression> argumentList = new ArrayList<>();

            public Annotation(Expression expression) {
                this.expression = expression;
            }

            @Override
            public void accept(Visitor visitor) {
                visitor.visit(this);
            }
        }

        public static final class Modifier extends CJTree implements Comparable<Modifier> {
            public final String text;
            public final Modifiers value;

            public Modifier(Modifiers value) {
                this.value = value;
                this.text = value.text;
            }

            @Override
            public int compareTo(CJTree.Declaration.Modifier o) {
                return value.compareTo(o.value);
            }

            @Override
            public boolean equals(Object o) {
                if (this == o) {
                    return true;
                }
                if (!(o instanceof Modifier modifier)) {
                    return false;
                }
                return value == modifier.value;
            }

            @Override
            public int hashCode() {
                return Objects.hash(value);
            }

            @Override
            public void accept(Visitor visitor) {
                visitor.visit(this);
            }
        }

        public final boolean containsModifier(Modifiers modifier) {
            return modifiers.stream().map(m -> m.value).anyMatch(m -> m == modifier);
        }

        public abstract static sealed class TypeParameterOwnerDeclaration extends Declaration {
            private TypeArguments typeParameters;
            private List<GenericConstraint> constraints;

            public final TypeArguments typeParameters() {
                return typeParameters;
            }

            public final void addTypeParameter(Expression.Name.SimpleName.IdentifierName param) {
                if (typeParameters == null) {
                    typeParameters = new TypeArguments();
                }
                typeParameters.add(param);
            }

            public final List<GenericConstraint> constraints() {
                return constraints;
            }

            public final void addConstraint(GenericConstraint constraint) {
                if (constraints == null) {
                    constraints = new ArrayList<>(4);
                }
                constraints.add(constraint);
            }
        }

        public static final class TypeDeclaration extends TypeParameterOwnerDeclaration {
            public final TypeKind kind;
            public final List<Expression> supers = new ArrayList<>();
            public final List<CJTree> declarations = new ArrayList<>();

            private Expression.Name.SimpleName.IdentifierName type;

            public TypeDeclaration(TypeKind kind) {
                this.kind = kind;
                check(supported(kind));
            }

            public Expression.Name.SimpleName.IdentifierName getType() {
                return type;
            }

            public void setType(Expression.Name.SimpleName.IdentifierName type) {
                this.type = type;
            }

            private static boolean supported(TypeKind kind) {
                return switch (kind) {
                    case CLASS, STRUCT, INTERFACE, EXTEND, ANNOTATION -> true;
                    case ENUM, TYPE_PARAMETER, TUPLE, FUNCTION -> false;
                };
            }

            @Override
            public void accept(Visitor visitor) {
                visitor.visit(this);
            }
        }

        public static final class FunctionDeclaration extends TypeParameterOwnerDeclaration {
            public final String name;
            public final boolean isConstructor;
            public final List<VariableDeclaration.Parameter> valueParameters = new ArrayList<>();

            private Expression.Name returnType;
            private Expression.Block body;

            public FunctionDeclaration(String name, boolean constructor) {
                this.name = name;
                this.isConstructor = constructor;
            }

            public Expression.Name getReturnType() {
                return returnType;
            }

            public void setReturnType(Expression.Name returnType) {
                this.returnType = returnType;
            }

            public Expression.Block getBody() {
                return body;
            }

            public void setBody(Expression.Block body) {
                this.body = body;
            }

            @Override
            public void accept(Visitor visitor) {
                visitor.visit(this);
            }
        }

        public abstract static sealed class VariableDeclaration extends Declaration {
            private String name;
            private Expression.Name type;
            private Expression initializer;

            protected VariableDeclaration(String name) {
                this.name = name;
            }

            public String getName() {
                return name;
            }

            public void setName(String name) {
                this.name = name;
            }

            public Expression.Name getType() {
                return type;
            }

            public void setType(Expression.Name type) {
                this.type = type;
            }

            public Expression getInitializer() {
                return initializer;
            }

            public void setInitializer(Expression initializer) {
                this.initializer = initializer;
            }

            public static final class Parameter extends VariableDeclaration {
                public Parameter(String name) {
                    super(name);
                }

                @Override
                public void accept(Visitor visitor) {
                    visitor.visit(this);
                }
            }

            public static final class Variable extends VariableDeclaration {
                private boolean let;
                private boolean prop;

                public Variable(String name, boolean prop) {
                    super(name);
                    this.prop = prop;
                }

                public boolean getLet() {
                    return let;
                }

                public void setLet(boolean let) {
                    this.let = let;
                }

                public boolean getProp() {
                    return prop;
                }

                public void setProp(boolean prop) {
                    this.prop = prop;
                }

                @Override
                public void accept(Visitor visitor) {
                    visitor.visit(this);
                }
            }
        }
    }

    public static final class TypeArguments extends CJTree {
        public final List<Expression.Name> arguments = new ArrayList<>();
        private boolean isHidden = false;

        public boolean shouldBeCommented() {
            return isHidden;
        }

        public void commentOut() {
            isHidden = true;
        }

        public void add(Expression.Name argument) {
            arguments.add(argument);
        }

        public void addAll(Collection<? extends Expression.Name> arguments) {
            this.arguments.addAll(arguments);
        }

        @Override
        public void accept(Visitor visitor) {
            visitor.visit(this);
        }
    }

    public static final class GenericConstraint extends CJTree {
        public final Expression.Name.SimpleName variable;
        public final List<Expression.Name> bounds = new ArrayList<>();

        public GenericConstraint(Expression.Name.SimpleName variable) {
            this.variable = variable;
        }

        @Override
        public void accept(Visitor visitor) {
            visitor.visit(this);
        }
    }

    public abstract static sealed class Expression extends CJTree {
        private Object importName; // null | QualifiedName | Collection<QualifiedName>

        @SuppressWarnings({"rawtypes", "unchecked"})
        private void addImport(Object o) {
            if (importName == null) {
                importName = o;
                return;
            }
            if (importName instanceof Collection collection) {
                collection.add(o);
                return;
            }
            final var list = new ArrayList<>(2);
            list.add(importName);
            list.add(o);
            importName = list;
        }

        public final void addImport(QualifiedName name) {
            addImport((Object) name);
        }

        public final Object importNames() {
            return importName;
        }

        public final boolean hasImportNames() {
            return importName != null;
        }

        public abstract static sealed class Literal extends Expression {
            public static final class Numeric extends Literal {
                public final java.lang.String text;
                public final Number value;

                public Numeric(Number value, java.lang.String text) {
                    this.value = value;
                    this.text = text;
                }

                @Override
                public void accept(Visitor visitor) {
                    visitor.visit(this);
                }
            }

            public static final class Rune extends Literal {
                public final char value;

                public Rune(char value) {
                    this.value = value;
                }

                @Override
                public void accept(Visitor visitor) {
                    visitor.visit(this);
                }
            }
            public static final class String extends Literal {
                public final java.lang.String value;

                public String(java.lang.String value) {
                    this.value = value;
                }

                @Override
                public void accept(Visitor visitor) {
                    visitor.visit(this);
                }
            }
            public static final class InterpolatedString extends Literal {
                public final List<Expression> parts;

                public InterpolatedString(List<Expression> parts) {
                    this.parts = parts;
                }

                @Override
                public void accept(Visitor visitor) {
                    visitor.visit(this);
                }
            }

            public static final class Singleton extends Literal {
                public final boolean value;

                private Singleton(boolean value) {
                    this.value = value;
                }

                @Override
                public void accept(Visitor visitor) {
                    visitor.visit(this);
                }
            }

            public static Literal bool(boolean value) {
                return new Singleton(value);
            }
        }

        public abstract static sealed class Name extends Expression {
            public static SimpleName.IdentifierName id(String s) {
                return new SimpleName.IdentifierName(s);
            }

            public static Name split(String name) {
                final var split = Utils.split(name, '.');
                Name left = new SimpleName.IdentifierName(split[0]);
                for (int i = 1; i < split.length; i++) {
                    left = new QualifiedName(left, new SimpleName.IdentifierName(split[i]));
                }
                return left;
            }

            public abstract static sealed class SimpleName extends Name {
                public static final class IdentifierName extends SimpleName {
                    public final String identifier;

                    public IdentifierName(String identifier) {
                        check(identifier.indexOf('.') == -1, identifier);
                        check(identifier.indexOf('$') == -1, identifier);
                        this.identifier = identifier;
                    }

                    @Override
                    public void accept(Visitor visitor) {
                        visitor.visit(this);
                    }
                }
                public static final class OptionName extends SimpleName {
                    private Name name;

                    public OptionName(Name name) {
                        this.name = name;
                    }

                    public Name getName() {
                        return name;
                    }

                    public void setName(Name name) {
                        this.name = name;
                    }

                    @Override
                    public void accept(Visitor visitor) {
                        visitor.visit(this);
                    }
                }
                public static final class GenericName extends SimpleName {
                    public final String identifier;
                    public final TypeArguments arguments = new TypeArguments();

                    public GenericName(String name) {
                        this.identifier = name;
                    }

                    @Override
                    public void accept(Visitor visitor) {
                        visitor.visit(this);
                    }
                }

                public static final class ErasedTypeVariableName extends SimpleName {
                    public final IdentifierName identifier;
                    public final Name erasedTypeName;
                    private boolean shouldBeCommented = false;

                    public ErasedTypeVariableName(IdentifierName identifier, Name erasedTypeName) {
                        this.identifier = identifier;
                        this.erasedTypeName = erasedTypeName;
                    }

                    public boolean shouldBeCommented() {
                        return shouldBeCommented;
                    }

                    public void commentOut() {
                        shouldBeCommented = true;
                    }

                    @Override
                    public void accept(Visitor visitor) {
                        visitor.visit(this);
                    }
                }
            }

            public static final class VariancedTypeName extends Name {
                public final Name typeName;

                public enum Variance {
                    IN("in "),

                    OUT("out ");

                    public final String qualifier;

                    Variance(String qualifier) {
                        this.qualifier = qualifier;
                    }
                }

                public final Variance variance;

                public VariancedTypeName(Name typeName, Variance variance) {
                    this.typeName = typeName;
                    this.variance = variance;
                }

                @Override
                public void accept(Visitor visitor) {
                    visitor.visit(this);
                }
            }

            public static final class QualifiedName extends Name {
                private Name left;
                private SimpleName right;

                public QualifiedName(Name left, SimpleName right) {
                    this.left = left;
                    this.right = right;
                }

                public Name getLeft() {
                    return left;
                }

                public void setLeft(Name left) {
                    this.left = left;
                }

                public SimpleName getRight() {
                    return right;
                }

                public void setRight(SimpleName right) {
                    this.right = right;
                }

                @Override
                public void accept(Visitor visitor) {
                    visitor.visit(this);
                }
            }
        }

        public static final class Block extends Expression {
            public final boolean preferMultiline;
            public final List<CJTree> expressions = new ArrayList<>();

            public Block(boolean preferMultiline) {
                this.preferMultiline = preferMultiline;
            }

            @Override
            public void accept(Visitor visitor) {
                visitor.visit(this);
            }
        }
    }
}
