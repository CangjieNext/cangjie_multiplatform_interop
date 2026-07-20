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

package cangjie.interop.cangjie.emit;

import cangjie.interop.cangjie.sema.TypeKind;
import cangjie.interop.cangjie.tree.CJTree;
import cangjie.interop.cangjie.tree.Modifiers;
import cangjie.interop.cangjie.tree.Translator;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.Objects;
import java.util.function.Predicate;
import java.util.stream.Collectors;

public final class CangjieEmitVisitor extends Translator<IndentedString> {
    private static final IndentedString EMPTY = new IndentedString();

    static {
        EMPTY.finish();
    }

    public IndentedString emit(CJTree tree) {
        return translate0(tree);
    }

    @Override
    protected IndentedString translate0(CJTree tree) {
        try {
            return super.translate0(tree);
        } catch (Throwable e) {
            // TODO: use Logger somehow
            e.printStackTrace();
            return span("<-- Internal source code emit error -->");
        }
    }

    private static IndentedString span() {
        return EMPTY;
    }

    private void append(IndentedString string, CJTree tree) {
        string.append(emit(tree));
    }

    private IndentedString span(Object arg) {
        final var result = new IndentedString();
        if (arg instanceof String s) {
            result.append(s);
        }
        if (arg instanceof Number n) {
            result.append(String.valueOf(n));
        }
        if (arg instanceof IndentedString s) {
            result.append(s);
        }
        if (arg instanceof CJTree tree) {
            append(result, tree);
        }
        result.finish();
        return result;
    }

    private IndentedString template(String template, Object... args) {
        final var result = new IndentedString();
        final Predicate<Object> processArg = arg -> {
            if (arg instanceof String s) {
                result.append(s);
                return true;
            }
            if (arg instanceof Number n) {
                result.append(String.valueOf(n));
                return true;
            }
            if (arg instanceof IndentedString s) {
                result.append(s);
                return true;
            }
            if (arg instanceof CJTree tree) {
                append(result, tree);
                return true;
            }
            return false;
        };
        int i = 0;
        for (
            int start, end;
            (start = template.indexOf('$', i)) != -1
                    && (end = template.indexOf('$', start + 1)) != -1;
            i = end + 1) {
            result.append(template.substring(i, start));

            final var sub = template.substring(start + 1, end);
            if (sub.equals("+")) {
                result.indentInc();
                continue;
            } else if (sub.equals("-")) {
                result.indentDec();
                continue;
            }
            final var colon = sub.indexOf(':');
            final String argIndex, delimiter;
            if (colon == -1) {
                argIndex = sub;
                delimiter = null;
            } else {
                argIndex = sub.substring(0, colon);
                delimiter = sub.substring(colon + 1);
            }
            final var arg = args[Integer.parseInt(argIndex)];
            if (processArg.test(arg)) {
                assert delimiter == null;
            } else if (arg instanceof Collection<?> collection) {
                assert delimiter != null;
                int j = 0;
                for (var iterator = collection.iterator(); iterator.hasNext(); j++) {
                    if (j != 0) {
                        result.append(delimiter);
                    }
                    final var item = iterator.next();
                    assert item != null;
                    boolean success = processArg.test(item);
                    assert success : item.getClass().getSimpleName();
                }
            } else {
                assert false : Objects.toString(arg);
            }
        }
        result.append(template.substring(i));
        result.finish();
        return result;
    }

    @Override
    public IndentedString translate(CJTree.CompilationUnit tree) {
        final var parts = new ArrayList<>();
        if (tree.getPackageName() != null) {
            parts.add(template("package $0$", tree.getPackageName()));
        }
        if (!tree.imports.isEmpty() || !tree.wildcardImports.isEmpty()) {
            final var importStatements = new ArrayList<>();
            for (final var anImport : tree.wildcardImports) {
                importStatements.add(span("import " + anImport.toString() + ".*"));
            }
            for (final var anImport : tree.imports) {
                importStatements.add(span(anImport.generateImport()));
            }
            parts.add(template("\n\n$0:\n$", importStatements));
        }

        for (var type : tree.types) {
            parts.add(template("\n\n$0$", type));
        }
        return template("$0:$", parts);
    }

    @Override
    public IndentedString translate(CJTree.Declaration.Annotation tree) {
        if (!tree.argumentList.isEmpty()) {
            return template("@$0$[$1:, $]", tree.expression, tree.argumentList);
        } else {
            return template("@$0$", tree.expression);
        }
    }

    @Override
    public IndentedString translate(CJTree.Declaration.Modifier tree) {
        return span(tree.text);
    }

    private IndentedString produceAnnotations(CJTree.Declaration tree) {
        return produceAnnotations(tree, "\n");
    }

    private IndentedString produceAnnotations(CJTree.Declaration tree, String delimiter) {
        if (tree.annotations.isEmpty()) {
            return span();
        }
        return template("$0:" + delimiter + "$" + delimiter, tree.annotations);
    }

    private IndentedString produceModifiers(CJTree.Declaration tree) {
        if (tree.modifiers.isEmpty()) {
            return span();
        }
        return template("$0: $ ", tree.modifiers.stream().sorted().toList());
    }

    private IndentedString produceSupers(CJTree.Declaration.TypeDeclaration tree) {
        return tree.supers.isEmpty() ? span() : template(" <: $0: & $", tree.supers);
    }

    private IndentedString produceConstraints(CJTree.Declaration.TypeParameterOwnerDeclaration tree) {
        final var constraints = tree.constraints();
        if (constraints == null || constraints.isEmpty()) {
            return span();
        }
        final var template = " where $0:, $";
        final var commentOut = tree.typeParameters() != null && tree.typeParameters().shouldBeCommented();
        return template(commentOut ? "/*" + template + "*/" : template, tree.constraints());
    }

    @Override
    public IndentedString translate(CJTree.Declaration.TypeDeclaration tree) {
        final var result = new IndentedString();
        result.append(translateDeclarationPrefix(tree));
        final Object[] args = {
                produceTypeParams(tree), produceSupers(tree), produceConstraints(tree),
                adjustDeclFormatting(tree.declarations)
        };
        result.append(template("$0$$1$$2$ {$+$$3:$$-$\n}", args));
        result.finish();
        return result;
    }

    private IndentedString translateDeclarationPrefix(CJTree.Declaration.TypeDeclaration tree) {
        final var symbolKind = tree.kind;
        final String kindKeyword = switch (symbolKind) {
            case CLASS -> "class";
            case STRUCT -> "struct";
            case INTERFACE, ANNOTATION -> "interface";
            case EXTEND -> "extend";
            default -> throw new IllegalStateException("Unexpected value: " + tree.kind);
        };
        final Object[] args = {
                               produceAnnotations(tree),
                               produceModifiers(tree),
                               kindKeyword, produceTypeParams(tree),
                               tree.getType()};
        return symbolKind == TypeKind.EXTEND ? template("$0$$1$$2$$3$ $4$", args) : template("$0$$1$$2$ $4$", args);
    }

    private IndentedString produceTypeParams(CJTree.Declaration.TypeParameterOwnerDeclaration tree) {
        return tree.typeParameters() != null ? template("$0$", tree.typeParameters()) : span();
    }

    @Override
    public IndentedString translate(CJTree.Declaration.FunctionDeclaration tree) {
        final var func = tree.isConstructor ? "" : "func ";
        final StringBuilder template = new StringBuilder("$0$$1$$2$$3$$4$");
        if (tree.containsModifier(Modifiers.OPERATOR)) {
            template.append(' ');
        }
        template.append("($5:, $)");
        if (tree.getReturnType() != null) {
            template.append(": $6$");
        }
        template.append("$7$");
        if (tree.getBody() != null) {
            template.append(" $8$");
        }
        return template(
                        template.toString(),
                        produceAnnotations(tree),
                        produceModifiers(tree),
                        func,
                        tree.name,
                        produceTypeParams(tree),
                        tree.valueParameters,
                        tree.getReturnType(),
                        produceConstraints(tree),
                        tree.getBody());
    }

    private IndentedString produce0(CJTree.Declaration.VariableDeclaration tree) {
        final var name = tree.getName() != null ? tree.getName() : "<NO NAME>";
        final Object[] args = {name, span(), tree.getType(), tree.getInitializer()};
        if (tree.getType() != null && tree.getInitializer() != null) {
            return template("$0$$1$: $2$ = $3$", args);
        }
        assert tree.getType() != null || tree.getInitializer() != null;
        if (tree.getType() != null) {
            return template("$0$: $2$", args);
        }
        if (tree.getInitializer() != null) {
            return template("$0$ = $3$", args);
        }
        return null;
    }

    @Override
    public IndentedString translate(CJTree.Declaration.VariableDeclaration.Parameter tree) {
        final Object[] args = {produceAnnotations(tree, " "), produce0(tree)};
        return template("$0$$1$", args);
    }

    @Override
    public IndentedString translate(CJTree.Declaration.VariableDeclaration.Variable tree) {
        if (tree.getProp()) {
            final Object[] args = {produceAnnotations(tree), produceModifiers(tree), tree.getLet() ? "" : "mut ",
                                   tree.getName(), tree.getType(), tree.getInitializer()};

            final StringBuilder pattern = new StringBuilder("$0$$1$$2$prop $3$: $4$");
            if (tree.getInitializer() != null) {
                pattern.append(" {$+$\nget() { $5$ }");
                pattern.append(tree.getLet() ? "" : "\nset(value) { }");
                pattern.append("$-$\n}");
            }

            return template(pattern.toString(), args);
        } else {
            final Object[] args = {produceAnnotations(tree), produceModifiers(tree), tree.getLet() ? "let" : "var",
                                   produce0(tree)};
            return template("$0$$1$$2$ $3$", args);
        }
    }

    @Override
    public IndentedString translate(CJTree.GenericConstraint tree) {
        return template("$0$ <: $1: & $", tree.variable, tree.bounds);
    }

    @Override
    public IndentedString translate(CJTree.TypeArguments tree) {
        if (tree.shouldBeCommented()) {
            return template("/*<$0:, $>*/", tree.arguments);
        }
        return template("<$0:, $>", tree.arguments);
    }

    private static String quote(int codePoint) {
        return switch (codePoint) {
            case '\b' -> "\\b";
            case '\f' -> "\\f";
            case '\n' -> "\\n";
            case '\r' -> "\\r";
            case '\t' -> "\\t";
            case '\'' -> "\\'";
            case '\"' -> "\\\"";
            case '\\' -> "\\\\";
            default -> Character.isDefined(codePoint)
                    && !Character.isISOControl(codePoint)
                    && Character.getType(codePoint) != Character.SURROGATE
                       ? Character.toString(codePoint)
                       : String.format("\\u{%04x}", codePoint);
        };
    }

    private static String quote(String value) {
        return value.codePoints()
                .mapToObj(CangjieEmitVisitor::quote)
                .collect(Collectors.joining());
    }

    @Override
    public IndentedString translate(CJTree.Expression.Literal.Numeric tree) {
        return span(tree.text);
    }

    @Override
    public IndentedString translate(CJTree.Expression.Literal.Rune tree) {
        return template("r'$0$'", quote(tree.value));
    }

    @Override
    public IndentedString translate(CJTree.Expression.Literal.String tree) {
        return template("\"$0$\"", quote(tree.value));
    }

    @Override
    public IndentedString translate(CJTree.Expression.Literal.InterpolatedString tree) {
        final var result = new IndentedString();
        result.append("\"");
        for (final var part : tree.parts) {
            if (part instanceof CJTree.Expression.Literal.String str) {
                result.append(quote(str.value));
            } else if (part instanceof CJTree.Expression.Literal.Rune str) {
                result.append(quote(str.value));
            } else {
                result.append("${");
                result.append(emit(part));
                result.append("}");
            }
        }
        result.append("\"");
        result.finish();
        return result;
    }

    @Override
    public IndentedString translate(CJTree.Expression.Literal.Singleton tree) {
        return span(Boolean.toString(tree.value));
    }

    @Override
    public IndentedString translate(CJTree.Expression.Name.SimpleName.IdentifierName tree) {
        return span(tree.identifier);
    }

    @Override
    public IndentedString translate(CJTree.Expression.Name.SimpleName.OptionName tree) {
        return template("?$0$", tree.getName());
    }

    @Override
    public IndentedString translate(CJTree.Expression.Name.SimpleName.GenericName tree) {
        return template("$0$$1$", tree.identifier, tree.arguments);
    }

    @Override
    public IndentedString translate(CJTree.Expression.Name.SimpleName.FreeTypeVariableName tree) {
        if (tree.shouldBeCommented()) {
            return template("$0$ /*$1$*/", tree.erasedTypeName, tree.identifier);
        }
        return span(tree.identifier);
    }

    @Override
    public IndentedString translate(CJTree.Expression.Name.VariancedTypeName tree) {
        return template("$0$$1$", tree.variance.qualifier, tree.typeName);
    }

    @Override
    public IndentedString translate(CJTree.Expression.Name.QualifiedName tree) {
        return template("$0$.$1$", tree.getLeft(), tree.getRight());
    }

    private IndentedString produceEmpty() {
        return span("{ }");
    }

    private IndentedString produceMultiple(CJTree.Expression.Block tree) {
        return template("{$+$$0:$$-$\n}", adjustStmtFormatting(tree.expressions));
    }

    @Override
    public IndentedString translate(CJTree.Expression.Block tree) {
        assert tree.expressions.stream().noneMatch(Objects::isNull);
        if (tree.preferMultiline) {
            return tree.expressions.isEmpty() ? produceEmpty() : produceMultiple(tree);
        }
        return switch (tree.expressions.size()) {
            case 0 -> produceEmpty();
            case 1 -> template("{ $0$ }", tree.expressions.get(0));
            default -> produceMultiple(tree);
        };
    }

    @Override
    public IndentedString translateDefault(CJTree tree) {
        throw new RuntimeException("Not implemented: " + tree.getClass().getName());
    }

    private List<Object> adjustDeclFormatting(List<CJTree> trees) {
        final var parts = new ArrayList<Object>();
        var isFirst = true;
        for (var decl : trees) {
            parts.add(isFirst ? "\n" : "\n\n");
            parts.add(decl);
            isFirst = false;
        }
        return parts;
    }

    private List<Object> adjustStmtFormatting(List<CJTree> trees) {
        final var parts = new ArrayList<Object>();
        for (var tree : trees) {
            parts.add("\n");
            parts.add(tree);
        }
        return parts;
    }
}
