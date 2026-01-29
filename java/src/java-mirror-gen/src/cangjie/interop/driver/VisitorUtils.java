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

package cangjie.interop.driver;

import static cangjie.interop.Utils.addBackticksIfNeeded;
import static cangjie.interop.Utils.addUnderscoresIfNeeded;
import static cangjie.interop.Utils.getFlatNameWithoutPackage;
import static cangjie.interop.util.NotNullAnnotations.NOT_NULL_ANNOTATIONS;
import static vendor.com.sun.tools.javac.code.Flags.EFFECTIVELY_FINAL;
import static vendor.com.sun.tools.javac.code.Flags.FINAL;
import static vendor.com.sun.tools.javac.code.Kinds.Kind.TYP;
import static vendor.com.sun.tools.javac.code.Kinds.KindSelector.VAL_MTH;
import static vendor.com.sun.tools.javac.code.TypeTag.TYPEVAR;
import static vendor.javax.lang.model.element.Modifier.PUBLIC;
import static vendor.javax.lang.model.element.Modifier.PROTECTED;
import static vendor.javax.lang.model.element.Modifier.PRIVATE;

import cangjie.interop.cangjie.QualifiedName;
import cangjie.interop.cangjie.tree.CJTree;
import cangjie.interop.cangjie.tree.Scanner;
import vendor.com.sun.tools.javac.code.Attribute;
import vendor.com.sun.tools.javac.code.Symbol;
import vendor.com.sun.tools.javac.code.Symtab;
import vendor.com.sun.tools.javac.code.TargetType;
import vendor.com.sun.tools.javac.code.Type;
import vendor.com.sun.tools.javac.code.Types;
import vendor.com.sun.tools.javac.tree.JCTree;
import vendor.com.sun.tools.javac.tree.TreeInfo;
import vendor.com.sun.tools.javac.util.Convert;
import vendor.com.sun.tools.javac.util.Name;
import vendor.com.sun.tools.javac.util.Names;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.LinkedHashSet;
import java.util.Map;
import java.util.Optional;
import java.util.Set;

public final class VisitorUtils {
    private static Symtab symtab;
    private static Names names;
    private static final Set<Symbol.ClassSymbol> symbolsToMangle = new HashSet<>();
    private static Map<String, String> importsMap = new HashMap<>();

    private VisitorUtils() {
    }

    public static Set<Symbol.ClassSymbol> getSymbolsToMangle() {
        return symbolsToMangle;
    }

    public static void addSymbolsToMangle(Symbol.ClassSymbol sym) {
        symbolsToMangle.add(sym);
    }

    public static void setSymtab(Symtab symtab) {
        VisitorUtils.symtab = symtab;
    }

    public static void setNames(Names names) {
        VisitorUtils.names = names;
    }

    public static void setImportsMap(Map<String, String> importsMap) {
        VisitorUtils.importsMap = importsMap;
    }

    static boolean isVarFinal(JCTree tree) {
        return isVarFinal(TreeInfo.symbolFor(tree));
    }

    static boolean isVarFinal(Symbol symbol) {
        if (symbol instanceof Symbol.VarSymbol var) {
            // TODO: option to disable treating effectively final variables as final?
            return (var.flags() & (FINAL | EFFECTIVELY_FINAL)) != 0;
        }
        return false;
    }

    public static CJTree.Expression defaultValueForType(Type type) {
        return switch (type.getKind()) {
            case BOOLEAN -> CJTree.Expression.Literal.bool(false);
            case BYTE, SHORT, INT, LONG, CHAR -> new CJTree.Expression.Literal.Numeric(0, "0");
            case FLOAT, DOUBLE -> new CJTree.Expression.Literal.Numeric(0.0, "0.0");
            default -> defaultValueForType();
        };
    }

    public static CJTree.Expression.Name.SimpleName.IdentifierName defaultValueForType() {
        return new CJTree.Expression.Name.SimpleName.IdentifierName("None");
    }

    public static CJTree.Expression.Name.SimpleName.IdentifierName defaultValueForToString() {
        return new CJTree.Expression.Name.SimpleName.IdentifierName("JString()");
    }

    public static CJTree.Expression.Name name(Type type) {
        return name(type, false);
    }

    public static CJTree.Expression.Name name(Type type, boolean isNotNull) {
        CJTree.Expression.Name result;
        if (type.isPrimitiveOrVoid()) {
            final var ident =
                    switch (type.getKind()) {
                        case BOOLEAN -> "Bool";
                        case BYTE -> "Int8";
                        case SHORT -> "Int16";
                        case INT -> "Int32";
                        case LONG -> "Int64";
                        case CHAR -> "UInt16";
                        case FLOAT -> "Float32";
                        case DOUBLE -> "Float64";
                        case VOID -> "Unit";
                        default -> null;
                    };
            if (ident == null) {
                throw new RuntimeException("Unsupported type");
            }
            return new CJTree.Expression.Name.SimpleName.IdentifierName(ident);
        }

        if (type instanceof Type.ArrayType arrayType) {
            var elemType = arrayType.getComponentType();
            final var translatedElemType = name(elemType);
            final var ident = new CJTree.Expression.Name.SimpleName.GenericName("JArray");
            ident.arguments.add(translatedElemType);
            result = isNotNull
                    ? ident
                    : new CJTree.Expression.Name.SimpleName.OptionName(ident);

            if (translatedElemType.importNames() instanceof QualifiedName qualifiedName) {
                result.addImport(qualifiedName);
            }
            final Optional<QualifiedName> clazz = QualifiedName.parse("java.lang.JArray");
            clazz.ifPresent(result::addImport);
            return result;
        }

        if (type.tsym instanceof Symbol.ClassSymbol typeSymbol) {
            String identifier;
            String qualifiedName;
            if (type.tsym == symtab.objectType.tsym) {
                identifier = "JObject";
                qualifiedName = "java.lang." + identifier;
            } else if (type.tsym == symtab.stringType.tsym) {
                identifier = "JString";
                qualifiedName = "java.lang." + identifier;
            } else {
                identifier = symbolsToMangle.contains(typeSymbol)
                        ? mangleClassName(typeSymbol)
                        : addBackticksIfNeeded(getFlatNameWithoutPackage(typeSymbol));
                qualifiedName = addBackticksIfNeeded(typeSymbol.flatName());

                final var nameFromMap = importsMap.get(typeSymbol.flatName().toString());
                if (nameFromMap != null) {
                    identifier = addUnderscoresIfNeeded(Convert.shortName(names.fromString(nameFromMap)));
                    qualifiedName = nameFromMap;
                }
            }

            CJTree.Expression.Name.SimpleName ident;

            ident = new CJTree.Expression.Name.SimpleName.IdentifierName(identifier);
            final Optional<QualifiedName> optClazz = QualifiedName.parse(qualifiedName);
            if (optClazz.isPresent()) {
                final QualifiedName clazz = optClazz.get();
                if (clazz.fullPackageName() != null) {
                    ident.addImport(clazz);
                }
            }

            return isNotNull
                    ? ident
                    : new CJTree.Expression.Name.SimpleName.OptionName(ident);
        }

        return null;
    }

    public static void collectImports(CJTree.CompilationUnit unit, QualifiedName packageName) {
        final var currentImportedNames = new LinkedHashSet<QualifiedName>();

        new Scanner() {
            @Override
            protected void scan(CJTree tree) {
                if (tree instanceof CJTree.Expression expression) {
                    final var importName = expression.importNames();
                    if (importName instanceof Collection<?> collection) {
                        for (final var o : collection) {
                            registerImport(o);
                        }
                    } else if (importName != null) {
                        registerImport(importName);
                    }
                }
                super.scan(tree);
            }

            private void registerImport(Object importName) {
                if (importName instanceof QualifiedName name) {
                    registerImport(name);
                }
            }

            private void registerImport(QualifiedName name) {
                currentImportedNames.add(name);
            }
        }.scan(unit);

        final var filtered = new ArrayList<QualifiedName>();
        for (final var name : currentImportedNames) {
            if (!name.requiresImport() || name.isFromPackage(packageName)) {
                continue;
            }
            filtered.add(name);
        }
        filtered.sort(Comparator.comparing(QualifiedName::toString));
        unit.imports.addAll(filtered);
    }

    public static Symbol.TypeSymbol erasureType(Type type, Types types) {
        var result = type;
        while (result instanceof Type.ArrayType arrayType) {
            result = arrayType.elemtype;
        }
        return types.erasure(result).tsym;
    }

    public static CJTree.Expression createJavaMirrorAnnotation() {
        return new CJTree.Expression.Name.SimpleName.IdentifierName("JavaMirror");
    }

    public static String mangleClassName(Symbol.ClassSymbol typeSymbol) {
        return addUnderscoresIfNeeded(typeSymbol.flatName()).replace('.', '_');
    }

    public static CJTree.Declaration.Annotation foreignNameAnnotationGen(Name originalName) {
        final var foreignNameAnnotation =
                new CJTree.Expression.Name.SimpleName.IdentifierName("ForeignName");
        final var annotation = new CJTree.Declaration.Annotation(foreignNameAnnotation);
        final var argument = new CJTree.Expression.Literal.String(originalName.toString());
        annotation.argumentList.add(argument);
        return annotation;
    }

    public static CJTree.Declaration.Annotation defaultMethodAnnotationGen() {
        final var defaultMethodAnnotation = new CJTree.Expression.Name.SimpleName.IdentifierName("JavaHasDefault");
        return new CJTree.Declaration.Annotation(defaultMethodAnnotation);
    }

    public static Name formTypeName(Symbol.TypeSymbol type) {
        Symbol symbol = type;
        Name result = null;
        char sep = 0;
        do {
            result = prependTypeNameComponent(symbol, sep, result);

            final var owner = symbol.owner;
            if (owner == null || owner.kind.matches(VAL_MTH) || (owner.kind == TYP && owner.type.hasTag(TYPEVAR))) {
                return result;
            }

            sep = owner.kind == TYP ? '$' : '.';
            symbol = owner;
        } while (true);
    }

    private static Name prependTypeNameComponent(Symbol symbol, char sep, Name suffix) {
        var name = symbol.name;
        if (name.lastIndexOf((byte) '$') != -1) {
            name = name.table.fromString(name.toString().replace("$", "\\$"));
        }

        if (name != null && name != name.table.names.empty) {
            return suffix != null ? name.append(sep, suffix) : name;
        }

        return suffix;
    }

    /**
     * Determines whether the given entity should be generated in the output.
     *
     * @param symbol the entity to check
     * @return true if the entity should be generated, false otherwise
     */
    public static boolean shouldBeGenerated(Symbol symbol) {
        if (hasAppropriateModifiers(symbol)) {
            return true;
        }
        if (symbol instanceof Symbol.ClassSymbol classSymbol && isPackagePrivate(classSymbol)) {
            return classSymbol.members().anyMatch(s -> hasAppropriateModifiers(s));
        }
        return false;
    }

    private static boolean hasAppropriateModifiers(Symbol symbol) {
        final var modifiers = symbol.getModifiers();
        return modifiers.contains(PUBLIC) || modifiers.contains(PROTECTED);
    }

    public static boolean isPackagePrivate(Symbol symbol) {
        if (symbol == null) {
            return false;
        }
        final var modifiers = symbol.getModifiers();
        return !modifiers.contains(PUBLIC) && !modifiers.contains(PROTECTED) && !modifiers.contains(PRIVATE);
    }

    public static Symbol.ClassSymbol firstAppropriateSuperClass(Symbol.TypeSymbol symbol) {
        if (!(symbol instanceof Symbol.ClassSymbol classSymbol)) {
            return null;
        }

        final var superClass = classSymbol.getSuperclass();
        if (superClass == null || superClass.tsym == null) {
            return null;
        }
        if (!(superClass.tsym instanceof Symbol.ClassSymbol superClassSymbol)) {
            return null;
        }
        if (shouldBeGenerated(superClassSymbol)) {
            return superClassSymbol;
        }
        return firstAppropriateSuperClass(superClassSymbol);
    }

    public static boolean isAppropriateType(Symbol.TypeSymbol typeSymbol) {
        if (typeSymbol == symtab.objectType.tsym) {
            return true;
        }
        if (typeSymbol == symtab.stringType.tsym) {
            return true;
        }
        if (typeSymbol.type.isPrimitiveOrVoid()) {
            return true;
        }
        if (typeSymbol.type instanceof Type.ArrayType arrayType) {
            var elemType = arrayType.getComponentType();
            return isAppropriateType(elemType.tsym);
        }
        if (!(typeSymbol instanceof Symbol.ClassSymbol classSymbol)) {
            return true;
        }
        return hasAppropriateModifiers(typeSymbol);
    }

    /**
     * Determines whether the given entity's dependencies are all appropriate.
     *
     * @param symbol the entity to check
     * @param types  the compiler utility
     * @return true if all dependencies are appropriate, false otherwise
     */
    public static boolean hasOnlyAppropriateDeps(Symbol symbol, Types types) {
        if (symbol instanceof Symbol.MethodSymbol methodSymbol) {
            for (var param : methodSymbol.params()) {
                final var paramTypeSymbol = erasureType(param.type, types);
                if (!isAppropriateType(paramTypeSymbol)) {
                    return false;
                }
            }
            final var returnTypeSymbol = erasureType(methodSymbol.getReturnType(), types);
            return isAppropriateType(returnTypeSymbol);
        }
        if (symbol instanceof Symbol.VarSymbol varSymbol) {
            return isAppropriateType(erasureType(varSymbol.type, types));
        }
        return true;
    }

    public static List<Type> collectSuperTypes(Symbol.ClassSymbol classSymbol, Types types) {
        List<Type> result = new ArrayList<>();
        final var appropriateSuperClass = firstAppropriateSuperClass(classSymbol);
        if (appropriateSuperClass != null) {
            result.add(appropriateSuperClass.type);
        }
        result.addAll(collectSuperInterfaces(classSymbol, types));
        return result;
    }

    public static Collection<Type> collectSuperInterfaces(Symbol.ClassSymbol classSymbol, Types types) {
        LinkedHashSet<Type> result = new LinkedHashSet<>();
        final var superInterfaces = classSymbol.getInterfaces();
        for (Type superInterface : superInterfaces) {
            if (shouldBeGenerated(superInterface.tsym)) {
                if (!result.contains(superInterface)) {
                    result.add(superInterface);
                }
                continue;
            }
            if (isPackagePrivate(superInterface.tsym)) {
                if (!(superInterface.tsym instanceof Symbol.ClassSymbol superInterfaceSymbol)) {
                    continue;
                }
                final var packagePrivateSupers = collectSuperInterfaces(superInterfaceSymbol, types);
                result.addAll(packagePrivateSupers.stream().filter(type -> !result.contains(type)).toList());
            }
        }
        return result;
    }

    private static boolean isAnnotatedSymbol(Attribute.Compound attribute) {
        return (attribute != null && attribute.type.tsym instanceof Symbol.ClassSymbol classSymbol)
                && NOT_NULL_ANNOTATIONS.contains(classSymbol.flatname.toString());
    }

    public static boolean hasNotNullAnnotation(Symbol symbol) {
        return symbol.getDeclarationAttributes().stream().anyMatch(a -> isAnnotatedSymbol(a));
    }

    public static boolean hasNotNullSigParamAnnotation(Symbol symbol, TargetType targetType, int index) {
        return symbol.getRawTypeAttributes().stream().anyMatch(a ->
                isAnnotatedSymbol(a) &&
                        a.position.type == targetType &&
                        (targetType == TargetType.METHOD_FORMAL_PARAMETER && a.position.parameter_index == index));
    }

    public static boolean hasNotNullTypeAnnotation(Symbol symbol, TargetType targetType) {
        return symbol.getRawTypeAttributes().stream().anyMatch(a ->
                isAnnotatedSymbol(a) &&
                        a.position.type == targetType);
    }
}
