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
import static cangjie.interop.Utils.syntheticParameterName;
import static cangjie.interop.driver.VisitorUtils.addSymbolsToMangle;
import static cangjie.interop.driver.VisitorUtils.collectImports;
import static cangjie.interop.driver.VisitorUtils.collectSuperTypes;
import static cangjie.interop.driver.VisitorUtils.createJavaMirrorAnnotation;
import static cangjie.interop.driver.VisitorUtils.defaultMethodAnnotationGen;
import static cangjie.interop.driver.VisitorUtils.defaultValueForToString;
import static cangjie.interop.driver.VisitorUtils.defaultValueForType;
import static cangjie.interop.driver.VisitorUtils.erasureType;
import static cangjie.interop.driver.VisitorUtils.foreignNameAnnotationGen;
import static cangjie.interop.driver.VisitorUtils.formTypeName;
import static cangjie.interop.driver.VisitorUtils.getSymbolsToMangle;
import static cangjie.interop.driver.VisitorUtils.hasOnlyAppropriateDeps;
import static cangjie.interop.driver.VisitorUtils.hasNotNullAnnotation;
import static cangjie.interop.driver.VisitorUtils.hasNotNullSigParamAnnotation;
import static cangjie.interop.driver.VisitorUtils.hasNotNullTypeAnnotation;
import static cangjie.interop.driver.VisitorUtils.isVarFinal;
import static cangjie.interop.driver.VisitorUtils.mangleClassName;
import static cangjie.interop.driver.VisitorUtils.name;
import static cangjie.interop.driver.VisitorUtils.setImportsMap;
import static cangjie.interop.driver.VisitorUtils.setNames;
import static cangjie.interop.driver.VisitorUtils.setSymtab;
import static cangjie.interop.driver.VisitorUtils.shouldBeGenerated;
import static cangjie.interop.util.StdCoreNames.STD_CORE_NAMES;
import static vendor.com.sun.tools.javac.code.Kinds.Kind.MTH;
import static vendor.com.sun.tools.javac.code.Kinds.Kind.VAR;
import static vendor.javax.tools.StandardLocation.CJ_OUTPUT;

import cangjie.interop.Utils;
import cangjie.interop.cangjie.QualifiedName;
import cangjie.interop.cangjie.tree.CJTree;
import cangjie.interop.cangjie.tree.Modifiers;
import cangjie.interop.driver.state.ParameterInfo;
import vendor.com.sun.tools.javac.code.Flags;
import vendor.com.sun.tools.javac.code.Scope;
import vendor.com.sun.tools.javac.code.Symbol;
import vendor.com.sun.tools.javac.code.Symtab;
import vendor.com.sun.tools.javac.code.TargetType;
import vendor.com.sun.tools.javac.code.Type;
import vendor.com.sun.tools.javac.code.Types;
import vendor.com.sun.tools.javac.util.Context;
import vendor.com.sun.tools.javac.util.Name;
import vendor.com.sun.tools.javac.util.Names;
import vendor.javax.lang.model.element.Modifier;
import vendor.javax.lang.model.type.TypeKind;
import vendor.javax.tools.JavaFileManager;
import vendor.javax.tools.JavaFileObject;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Paths;
import java.util.Arrays;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.Queue;
import java.util.Set;

public final class EmitMirrorVisitor {
    public final Queue<Symbol.ClassSymbol> queue;
    public final Set<Symbol.ClassSymbol> visited;
    private final HashMap<Symbol.ClassSymbol, Integer> traversalPathMap;
    private final JavaFileManager fileManager;
    private final Types types;
    private final Symtab symtab;
    private final MapScope scope;
    private final OverrideChains overrideChains;
    private final MemberHiding memberHiding;
    private final DefaultMethods defaultMethods;
    private final Names names;
    private final Name staticSuffix;
    private final Name hashCode32;
    private final Name toJString;
    private final HashMap<Symbol, Name> newName;
    private final boolean onePackageMode = System.getProperty("package.mode") != null;
    private final String userPackageName = System.getProperty("package.name", "UNNAMED");
    private final boolean generateDefinition =
            !System.getProperty("generate.definition", "false").equalsIgnoreCase("false");
    private final boolean generateAnnotationMode = onePackageMode && !generateDefinition
            || System.getProperty("generate.annotation") != null;
    private final boolean interfaceObjectMethodsWorkaround =
            System.getProperty("no.interface.jobject.workaround") == null;
    private final Map<String, List<String>> writtenPaths = new HashMap<>();
    private final Map<String, List<Symbol.ClassSymbol>> clashingClasses = new HashMap<>();
    private final Set<String> clashingFiles = new HashSet<>();
    private final Map<String, String> importsMap = new HashMap<>();
    private final String defaultImportsConfig = "imports_config.txt";
    private String importsConfig = System.getProperty("imports.config");
    private boolean limitedDepth = System.getProperty("gen.closure.depth") != null;
    private boolean considerNotNullAnnotations = System.getProperty("gen.notnull.types") != null;
    private int maxPathLength = Integer.MAX_VALUE;
    private Symbol.ClassSymbol currentClass;

    private EmitMirrorVisitor(Context context) {
        context.put(EmitMirrorVisitor.class, this);
        fileManager = context.get(JavaFileManager.class);
        queue = new LinkedList<>();
        visited = new LinkedHashSet<>();
        types = Types.instance(context);
        symtab = Symtab.instance(context);
        newName = new HashMap<>();
        scope = MapScope.instance(context);
        overrideChains = OverrideChains.instance(context);
        memberHiding = MemberHiding.instance(context);
        defaultMethods = DefaultMethods.instance(context);

        names = Names.instance(context);
        staticSuffix = names.fromString("Static");
        hashCode32 = names.fromString("hashCode32");
        toJString = names.fromString("toJString");

        setSymtab(symtab);
        setNames(names);

        traversalPathMap = new HashMap<>();
        if (limitedDepth) {
            traversalPathMap.put((Symbol.ClassSymbol) symtab.objectType.tsym, 0);
            traversalPathMap.put((Symbol.ClassSymbol) symtab.stringType.tsym, 0);
            try {
                maxPathLength = Integer.parseInt(System.getProperty("gen.closure.depth"));
            } catch (Exception e) {
                System.out.print(e.getMessage());
                limitedDepth = false;
                maxPathLength = Integer.MAX_VALUE;
            }
        }

        if (importsConfig != null) {
            if (importsConfig.isEmpty()) {
                importsConfig = defaultImportsConfig;
            }
            var importsConfigFile = new File(importsConfig);
            if (importsConfigFile.exists()) {
                try (BufferedReader br = new BufferedReader(new FileReader(importsConfig))) {
                    String str;
                    while ((str = br.readLine()) != null) {
                        String[] parts = str.split(" ");
                        if (parts.length != 2) {
                            continue;
                        }
                        importsMap.put(parts[0], parts[1]);
                    }
                } catch (IOException e) {
                    System.out.println("Error while reading a file.");
                }

                setImportsMap(importsMap);
            }
        }
    }

    public static EmitMirrorVisitor instance(Context context) {
        final var instance = context.get(EmitMirrorVisitor.class);
        return instance == null ? new EmitMirrorVisitor(context) : instance;
    }

    private static cangjie.interop.cangjie.sema.TypeKind kind(Symbol.ClassSymbol sym) {
        return switch (sym.getKind()) {
            case CLASS, ENUM, RECORD -> cangjie.interop.cangjie.sema.TypeKind.CLASS;
            case INTERFACE -> cangjie.interop.cangjie.sema.TypeKind.INTERFACE;
            case ANNOTATION_TYPE -> cangjie.interop.cangjie.sema.TypeKind.ANNOTATION;
            default -> throw new RuntimeException("Not implemented: " + sym.getKind());
        };
    }

    public void warnAboutOverwrite() {
        for (final var entry : writtenPaths.entrySet()) {
            final var value = entry.getValue();
            if (value.size() <= 1) {
                continue;
            }

            System.err.println("Multiple classes write to the same file `" + entry.getKey() + "`:");
            for (final var name : value) {
                System.err.println("* " + name);
            }
        }
    }

    public CJTree.Declaration translate(Symbol.MethodSymbol symbol) {
        if (symbol.owner.hasOuterInstance()) {
            // to generate arg in constructor for enclosing class
            symbol.flags_field = symbol.flags_field | Flags.SYNTHETIC;
        }
        var symbolImpl = symbol.implementation((Symbol.ClassSymbol) symbol.owner, types, true);
        if (symbolImpl == null) {
            symbolImpl = symbol;
        }

        // Let synthetic methods be those that don't exist in the .class file.
        // They will not belong to the current class - they come from its super types.
        final var isSynthetic = symbol.owner != currentClass;

        final var newMethodName = newName.get(symbol);
        final var methodName = newMethodName != null ? newMethodName.toString() : scope.generateMethodName(symbol);
        final var methodDecl = new CJTree.Declaration.FunctionDeclaration(methodName, symbol.isConstructor());

        final var infoList = ParameterInfo.makeParameters(scope.getParameters(symbol));

        int i = 0;
        for (var param : infoList) {
            final var decl = new CJTree.Declaration.VariableDeclaration.Parameter(param.name());

            if (i == 0 && param.isNameSynthetic() && symbol.isConstructor()) {
                final var classSymbol = symbol.owner;

                if (classSymbol.hasOuterInstance()) {
                    final var outerThisType = types.erasure(classSymbol.type.getEnclosingType());
                    decl.setType(name(outerThisType, true));
                }
            } else {
                Symbol.VarSymbol jParamSymbol = symbol.params().get(i);
                var erasureType = types.erasure(jParamSymbol.type);
                final var hasNotNullAttribute = considerNotNullAnnotations
                        && (hasNotNullAnnotation(jParamSymbol)
                            || hasNotNullSigParamAnnotation(symbol, TargetType.METHOD_FORMAL_PARAMETER, i));
                decl.setType(name(erasureType, hasNotNullAttribute));
                i++;
            }
            methodDecl.valueParameters.add(decl);
        }

        final var isRedundantModifier = currentClass.isInterface();
        final var javaModifiers = symbolImpl.getModifiers();
        final var cjModifiers = new ModifierList();
        if (javaModifiers.contains(Modifier.PUBLIC) && !isRedundantModifier) {
            cjModifiers.add(Modifiers.PUBLIC.toCJTree());
        }
        if (javaModifiers.contains(Modifier.PROTECTED)) {
            cjModifiers.add(Modifiers.PROTECTED.toCJTree());
        }

        var isAbstract = false;
        if (javaModifiers.contains(Modifier.ABSTRACT)) {
            // For synthetic methods both `symbol` and `symbolImpl` are arbitrary.
            // Do not trust the `abstract` modifier, do the full check in the current class.
            if (!isSynthetic || methodStillNotImplemented(symbolImpl)) {
                // Cangjie forbids overriding non-abstract method with an abstract one.
                // The best we can do is to ignore the `abstract` modifier.
                // This might lead to javac errors with incomplete @JavaImpl classes.
                if (!overrideChains.overridesNonAbstractMethod(symbolImpl)) {
                    isAbstract = true;
                }
            }
        }

        if (!generateDefinition && isAbstract && !isRedundantModifier) {
            cjModifiers.add(Modifiers.ABSTRACT.toCJTree());
        }

        if (generateDefinition && !isAbstract) {
            methodDecl.setBody(new CJTree.Expression.Block(true));
        }

        if (!symbol.isConstructor()) {
            final var isToString = Utils.isToString(symbol);

            if (javaModifiers.contains(Modifier.STATIC)) {
                cjModifiers.add(Modifiers.STATIC.toCJTree());
            } else {
                if (!symbol.enclClass().isFinal() && !javaModifiers.contains(Modifier.FINAL)
                        && !isRedundantModifier) {
                    cjModifiers.add(Modifiers.OPEN.toCJTree());
                }
            }
            var erasureType = types.erasure(symbol.getReturnType());
            final var hasNotNullAttribute = considerNotNullAnnotations
                    && (hasNotNullAnnotation(symbol)
                        || hasNotNullTypeAnnotation(symbol, TargetType.METHOD_RETURN));
            methodDecl.setReturnType(name(erasureType, hasNotNullAttribute));
            if (isToString &&
                    methodDecl.getReturnType() instanceof
                            CJTree.Expression.Name.SimpleName.OptionName methodReturnType) {
                methodDecl.setReturnType(methodReturnType.getName());
            }

            if (methodDecl.getBody() != null) {
                if (symbol.getReturnType().getKind() != TypeKind.VOID) {
                    CJTree.Expression returnValue = !isToString
                            ? defaultValueForType(symbol.getReturnType())
                            : defaultValueForToString();
                    methodDecl.getBody().expressions.add(returnValue);
                }
            }
        }

        methodDecl.modifiers.addAll(cjModifiers);

        return methodDecl;
    }

    private boolean methodStillNotImplemented(Symbol.MethodSymbol superMethod) {
        assert superMethod.getModifiers().contains(Modifier.ABSTRACT) : superMethod;

        var implMethod = superMethod.implementation(currentClass, types, true);
        if (implMethod == null || implMethod == superMethod) {
            // Search for default implementations.
            final var prov = types.interfaceCandidates(currentClass.type, superMethod).head;
            if (prov != null && overrideChains.overridesWithFilter(prov, superMethod)) {
                implMethod = prov;
            }
        }

        // Did we find the alternative symbol?
        return implMethod == null || implMethod == superMethod;
    }

    public CJTree translate(Symbol.ClassSymbol classSymbol) {
        assert shouldBeGenerated(classSymbol) : classSymbol;

        final var name = getSymbolsToMangle().contains(classSymbol)
                ? mangleClassName(classSymbol)
                : addBackticksIfNeeded(getFlatNameWithoutPackage(classSymbol));

        final var classDecl = new CJTree.Declaration.TypeDeclaration(kind(classSymbol));
        classDecl.setType(new CJTree.Expression.Name.SimpleName.IdentifierName(name));

        final var depth = traversalPathMap.get(classSymbol);

        final var superTypes = collectSuperTypes(classSymbol, types);
        for (Type superType : superTypes) {
            if (superType.tsym == symtab.objectType.tsym && (!generateDefinition || classSymbol.isInterface())) {
                // Interop toolchain provides implicit JObject super type, skip it.
                // Note: need to preserve JObject for non-interop toolchain.
                continue;
            }

            if (depth != null && depth == maxPathLength
                    && superType.tsym != symtab.objectType.tsym
                    && !traversalPathMap.containsKey(superType.tsym)) {
                continue;
            }

            final var superName = name(superType);

            if (superName instanceof CJTree.Expression.Name.SimpleName.OptionName optionName) {
                classDecl.supers.add(optionName.getName());
            }
        }

        if (generateAnnotationMode) {
            final var javaMirrorAnnotation = createJavaMirrorAnnotation();
            final var annotation = new CJTree.Declaration.Annotation(javaMirrorAnnotation);
            final var argument = new CJTree.Expression.Literal.String(formTypeName(classSymbol).toString());
            annotation.argumentList.add(argument);
            classDecl.annotations.add(annotation);
        }

        final var cjModifiers = new ModifierList();
        if (!classSymbol.getModifiers().contains(Modifier.FINAL) && !classSymbol.isInterface()
                && !classSymbol.isAbstract()) {
            cjModifiers.add(Modifiers.OPEN.toCJTree());
        }
        if (classSymbol.getModifiers().contains(Modifier.PUBLIC)) {
            cjModifiers.add(Modifiers.PUBLIC.toCJTree());
        }
        if (classSymbol.getModifiers().contains(Modifier.PROTECTED)) {
            cjModifiers.add(Modifiers.PROTECTED.toCJTree());
        }
        if (classSymbol.isAbstract() && !classSymbol.isInterface()) {
            cjModifiers.add(Modifiers.ABSTRACT.toCJTree());
        }
        classDecl.modifiers.addAll(cjModifiers);

        classDecl.declarations.addAll(visitBody(classSymbol));

        return classDecl;
    }

    public CJTree.Declaration translate(Symbol.VarSymbol varSymbol) {
        final var mangledName = addBackticksIfNeeded(varSymbol.name);
        boolean genProperty = currentClass.isInterface();
        final var decl = new CJTree.Declaration.VariableDeclaration.Variable(mangledName, genProperty);
        final var newVariableName = newName.get(varSymbol);
        if (newVariableName != null) {
            decl.setName(newVariableName.toString());
        }

        final var modifiers = new ModifierList();
        if (varSymbol.getModifiers().contains(Modifier.PUBLIC)) {
            modifiers.add(Modifiers.PUBLIC.toCJTree());
        }
        if (varSymbol.getModifiers().contains(Modifier.PROTECTED)) {
            modifiers.add(Modifiers.PROTECTED.toCJTree());
        }
        if (varSymbol.isStatic()) {
            modifiers.add(Modifiers.STATIC.toCJTree());
        }
        decl.modifiers.addAll(modifiers);

        decl.setLet(isVarFinal(varSymbol));
        var erasureType = types.erasure(varSymbol.type);
        final var hasNotNullAttribute = !generateDefinition
                && (hasNotNullAnnotation(varSymbol)
                    || hasNotNullTypeAnnotation(varSymbol, TargetType.FIELD));
        decl.setType(name(erasureType, hasNotNullAttribute));
        if (generateDefinition) {
            decl.setInitializer(defaultValueForType(erasureType));
        }
        return decl;
    }

    private void putToQueue(Symbol.TypeSymbol typeSymbol, Integer pathLevel) {
        if (pathLevel != null && pathLevel > maxPathLength) {
            return;
        }

        if (typeSymbol == symtab.objectType.tsym) {
            return;
        }
        if (typeSymbol == symtab.stringType.tsym) {
            return;
        }
        if (typeSymbol instanceof Symbol.ClassSymbol paramSymbol && !paramSymbol.type.isPrimitiveOrVoid()) {
            if (!shouldBeGenerated(typeSymbol)) {
                return;
            }
            queue.add(paramSymbol);
            if (limitedDepth && !traversalPathMap.containsKey(paramSymbol)) {
                traversalPathMap.put(paramSymbol, pathLevel);
            }
        }
    }

    private boolean exceedMaxDepth(Symbol.TypeSymbol symbol) {
        if (symbol instanceof Symbol.ClassSymbol classSymbol && !classSymbol.type.isPrimitiveOrVoid()) {
            return traversalPathMap.get(classSymbol) == null;
        }
        return false;
    }

    private boolean hasExceededMaxDepthPath(Symbol symbol, Integer depth) {
        if (!limitedDepth) {
            return false;
        }

        if (depth != null && symbol instanceof Symbol.MethodSymbol methodSymbol) {
            if (methodSymbol.owner.isInterface() && !methodSymbol.isDefault() && !methodSymbol.isStatic()) {
                for (Symbol.ClassSymbol classSymbol : traversalPathMap.keySet()) {
                    if (!classSymbol.isAbstract() && !classSymbol.isInterface()) {
                        final var closure = types.closure(classSymbol.type);
                        for (Type type : closure) {
                            if (type.tsym.equals(methodSymbol.owner)
                                    && !overrideChains.isMethodOverriddenInClass(classSymbol, methodSymbol)) {
                                return true;
                            }
                        }
                    }
                }
            }
        }

        if (depth != null && depth == maxPathLength) {
            if (symbol instanceof Symbol.MethodSymbol methodSymbol) {
                for (var param : methodSymbol.params()) {
                    final var paramTypeSymbol = erasureType(param.type, types);
                    if (exceedMaxDepth(paramTypeSymbol)) {
                        return true;
                    }
                }
                final var returnTypeSymbol = erasureType(methodSymbol.getReturnType(), types);
                return exceedMaxDepth(returnTypeSymbol);
            }
            if (symbol instanceof Symbol.VarSymbol varSymbol) {
                return exceedMaxDepth(erasureType(varSymbol.type, types));
            }
        }
        return false;
    }

    private ArrayList<Symbol> getMembers(Symbol.ClassSymbol classSymbol) {
        final var tmpScope = classSymbol.members();
        final var members = new ArrayList<Symbol>();
        final var depth = traversalPathMap.get(classSymbol);
        if (tmpScope == null) {
            return members;
        }

        final var visitedMethods = new LinkedHashSet<Symbol.MethodSymbol>();
        for (Symbol element : tmpScope.getSymbols()) {
            if (!shouldBeGenerated(element)) {
                continue;
            }
            if (element instanceof Symbol.MethodSymbol symbol) {
                // JObject methods cause problems in interfaces. Skip them.
                if (interfaceObjectMethodsWorkaround && classSymbol.isInterface()
                        && types.overridesObjectMethod(symbol.enclClass(), symbol)) {
                    continue;
                }

                if (!hasOnlyAppropriateDeps(symbol, types)) {
                    continue;
                }

                final var methodImpl = overrideChains.findRootMethod(symbol, classSymbol);

                final var methodSymbol = symbol.isStatic()
                        ? methodImpl
                        : overrideChains.getImplementationOfSuper(symbol, tmpScope);

                assert methodSymbol == symbol || !symbol.isStatic()
                        : classSymbol + " : " + symbol + " | " + methodSymbol;

                if ((symbol.flags() & Flags.BRIDGE) != 0
                        && methodSymbol == symbol
                        && overrideChains.isPackagePrivateOverridden(methodSymbol, classSymbol)) {
                    visitedMethods.add(methodSymbol);
                    continue;
                }

                if (hasExceededMaxDepthPath(methodSymbol, depth)) {
                    continue;
                }
                if (!hasOnlyAppropriateDeps(methodSymbol, types)) {
                    continue;
                }

                if (visitedMethods.add(methodSymbol)) {
                    members.add(methodSymbol);
                }
            } else if (element instanceof Symbol.VarSymbol varSymbol) {
                if (varSymbol.owner.type.isInterface() && varSymbol.getModifiers().contains(Modifier.FINAL)) {
                    continue;
                }

                if (hasExceededMaxDepthPath(varSymbol, depth)) {
                    continue;
                }
                if (!hasOnlyAppropriateDeps(varSymbol, types)) {
                    continue;
                }
                members.add(varSymbol);
            }
        }

        Collections.reverse(members);
        return members;
    }

    private List<CJTree.Declaration> visitBody(Symbol.ClassSymbol classSymbol) {
        final var block = new ArrayList<CJTree.Declaration>();

        boolean hasInitWithoutParams = false;
        boolean hasInit = false;

        final List<Symbol> members = getMembers(classSymbol);
        final var depth = traversalPathMap.get(classSymbol);

        final var mergeDefaultMethods = defaultMethods.mergeMultipleInterfaceDefaultMethods(classSymbol);
        mergeDefaultMethods.removeIf(members::contains);

        for (Symbol.MethodSymbol methodSymbol : mergeDefaultMethods) {
            if (hasExceededMaxDepthPath(methodSymbol, depth)) {
                continue;
            }
            members.add(methodSymbol);
        }
        for (Symbol element : members) {
            final var originalName = element.name;
            final var renamed = newName.get(element) != null;
            if (element instanceof Symbol.MethodSymbol methodSymbol) {
                if (!hasInitWithoutParams && methodSymbol.isConstructor()) {
                    if (methodSymbol.params().isEmpty()) {
                        hasInitWithoutParams = true;
                    }
                    hasInit = true;
                }

                final var tree = translate(methodSymbol);
                block.add(tree);

                if (generateAnnotationMode
                        && renamed
                        && overrideChains.isRenamedInThisClass(classSymbol, methodSymbol)) {
                    tree.annotations.add(foreignNameAnnotationGen(originalName));
                }

                if (generateAnnotationMode
                        && currentClass.isInterface()
                        && (methodSymbol.isDefault() && !methodSymbol.isStatic()
                            || methodSymbol.isAbstract() && overrideChains.overridesNonAbstractMethod(methodSymbol))) {
                    tree.annotations.add(defaultMethodAnnotationGen());
                }
            } else if (element instanceof Symbol.VarSymbol varSymbol) {
                final var tree = translate(varSymbol);
                block.add(tree);

                if (generateAnnotationMode && renamed) {
                    tree.annotations.add(foreignNameAnnotationGen(originalName));
                }
            }
        }
        if (!classSymbol.isInterface()
                && (generateDefinition && (!hasInitWithoutParams || classSymbol.hasOuterInstance())
                || !generateDefinition && !hasInit)) {
            final var initMethodTree = new CJTree.Declaration.FunctionDeclaration("init", true);
            initMethodTree.modifiers.add(Modifiers.PUBLIC.toCJTree());
            if (generateDefinition) {
                initMethodTree.setBody(new CJTree.Expression.Block(true));
            } else {
                if (classSymbol.hasOuterInstance() && !classSymbol.isStatic()) {
                    final var decl = new CJTree.Declaration.VariableDeclaration.Parameter(syntheticParameterName(0, 1));
                    final var outerThisType = types.erasure(classSymbol.type.getEnclosingType());
                    decl.setType(name(outerThisType));
                    initMethodTree.valueParameters.add(decl);
                }
            }
            block.add(initMethodTree);
        }
        return block;
    }

    private Collection<QualifiedName> filterImports(Set<QualifiedName> imports) {
        return onePackageMode ? List.of() : imports;
    }

    private Collection<String> getWildcardImports() {
        Set<String> wildcardImports = new HashSet<>();
        if (!onePackageMode) {
            return wildcardImports;
        }
        for (final var wildcardImport : importsMap.values()) {
            String[] parts = wildcardImport.split("\\.");
            if (parts.length < 1) {
                continue;
            }
            final var newNameParts = Arrays.copyOfRange(parts, 0, parts.length - 1);
            final var newPackageName = String.join(".", newNameParts);
            if (wildcardImports.contains(newPackageName)) {
                continue;
            }
            wildcardImports.add(newPackageName);
        }
        return wildcardImports;
    }

    private Symbol.PackageSymbol findPackageSymbol(Symbol.ClassSymbol classSymbol) {
        var ownerSymbol = classSymbol.owner;
        while (!(ownerSymbol instanceof Symbol.PackageSymbol)) {
            ownerSymbol = ownerSymbol.owner;
        }
        return (Symbol.PackageSymbol) ownerSymbol;
    }

    public void generateMirror(Symbol.ClassSymbol classSymbol) {
        if (classSymbol.classfile == null) {
            return;
        }
        if (classSymbol.hasOuterInstance() && classSymbol.isPrivate()) {
            return;
        }
        if (importsMap.containsKey(classSymbol.flatname.toString())) {
            return;
        }
        final var ownerSymbol = findPackageSymbol(classSymbol);
        final var moduleName = userPackageName != null ? userPackageName : "UNNAMED";
        final var unitPkgName = !onePackageMode ? ownerSymbol.getQualifiedName().toString() : moduleName;

        final var unit = new CJTree.CompilationUnit();
        final var tree = this.translate(classSymbol);

        unit.wildcardImports.add(QualifiedName.get("java", "lang"));
        for (final var wildcardImport : getWildcardImports()) {
            unit.wildcardImports.add(QualifiedName.get(wildcardImport));
        }

        unit.types.add(tree);

        unit.setPackageName(CJTree.Expression.Name.QualifiedName.split(
                !unitPkgName.isEmpty() ? unitPkgName : "UNNAMED"));

        final Optional<QualifiedName> optQualifiedName =
                QualifiedName.parse(!unitPkgName.isEmpty() ? unitPkgName : "UNNAMED");
        optQualifiedName.ifPresent(qn -> collectImports(unit, qn));
        final var filteredImports = filterImports(unit.imports);
        unit.imports.retainAll(filteredImports);

        final var path = new StringBuilder();
        assert moduleName != null;
        path.append(moduleName).append('/').append("src/");

        if (!onePackageMode) {
            path.append(unitPkgName.toString().replace('.', File.separatorChar)).append(File.separator);
        }

        final var filePath = new StringBuilder();
        final var fileName = Paths.get(classSymbol.classfile.getName()).getFileName().toString();
        final var lastDotIndex = fileName.lastIndexOf('.');
        if (lastDotIndex != -1) {
            filePath.append(fileName, 0, lastDotIndex);
        } else {
            filePath.append(fileName);
        }

        path.append(
                getSymbolsToMangle().contains(classSymbol) || clashingFiles.contains(filePath.toString())
                        ? mangleClassName(classSymbol)
                        : filePath
        );

        try {
            final var pathString = path.toString();
            writtenPaths.computeIfAbsent(pathString, k -> new ArrayList<>())
                    .add(classSymbol.getQualifiedName().toString());
            final var outFile = fileManager.getJavaFileForOutput(CJ_OUTPUT, pathString, JavaFileObject.Kind.CJ, null);

            try (final var out = outFile.openOutputStream();
                 final var writer = new OutputStreamWriter(out, StandardCharsets.UTF_8)) {
                final var indentedString = unit.produce();
                writer.write(indentedString.text());
            }
        } catch (IOException ex) {
            ex.printStackTrace();
        }
    }

    private void renameCangjieClashes(Scope.WriteableScope scope) {
        final var map = new HashMap<Name, List<Symbol>>();
        for (final var member : scope.getSymbols()) {
            List<Symbol> symbols = traverseInheritanceHierarchy(member, (Symbol.ClassSymbol) member.owner);

            for (Symbol symbol : symbols) {
                map.computeIfAbsent(symbol.name, k -> new ArrayList<>()).add(symbol);
            }
        }

        for (var symbols : map.values()) {
            final var fields = symbols.stream().filter(f -> f.kind == VAR).toList();
            if (fields.size() > 1) {
                for (var field : fields) {
                    final var mangledFieldName = Utils.addUnderscoresIfNeeded(field.name);
                    final var mangledFieldOwnerName = Utils.addUnderscoresIfNeeded(field.owner.name);

                    final var sb = new StringBuilder(mangledFieldName);
                    sb.append("_");
                    sb.append(mangledFieldOwnerName);

                    final var newFieldName = names.fromString(sb.toString());
                    newName.put(field, newFieldName);
                }
            }

            final var methods = symbols.stream().filter(m -> m.kind == MTH).toList();

            for (final var method : methods) {
                final var methodSymbol = (Symbol.MethodSymbol) method;
                if (Utils.isHashCode(methodSymbol)) {
                    newName.put(method, hashCode32);
                } else if (Utils.isToString(methodSymbol)) {
                    newName.put(method, toJString);
                }
            }

            if (methods.size() > 1) {
                final boolean allMethodsAreInstance = methods.stream().noneMatch(m -> m.isStatic());
                final boolean allMethodsAreStatic = methods.stream().allMatch(m -> m.isStatic());

                if (allMethodsAreInstance) {
                    continue;
                }
                for (var method : methods) {
                    if (method.isStatic()) {
                        final var newNameMethod = newName.get(method);
                        boolean shouldBeMangledStatic = !allMethodsAreStatic
                                || newNameMethod != null
                                && newNameMethod.startsWith(method.name.append(staticSuffix));
                        if (shouldBeMangledStatic) {
                            newName.put(method, method.name.append(staticSuffix));
                        }
                        // Cangjie prohibits redefinition of a static method with a different return type.
                        // Rename the descendant method.
                        final var methodSymbol = (Symbol.MethodSymbol) method;
                        final var hiddenSymbols = memberHiding.collectHiddenSymbols(method);

                        if (hiddenSymbols.isEmpty()) {
                            continue;
                        }

                        if (!hiddenSymbols.stream().anyMatch(s -> (s instanceof Symbol.MethodSymbol symbol)
                                && (!symbol.getReturnType().equals(methodSymbol.getReturnType())))) {
                            continue;
                        }

                        if (!(methodSymbol.getReturnType().tsym instanceof Symbol.ClassSymbol returnTypeSymbol)) {
                            continue;
                        }

                        final var returnTypeSuffix = mangleClassName(returnTypeSymbol);
                        final var nameSuffix = names.fromString(returnTypeSuffix);
                        newName.put(method, shouldBeMangledStatic
                                ? method.name.append(staticSuffix).append('_', nameSuffix)
                                : method.name.append('_', nameSuffix));
                    }
                }
            }
        }
    }

    List<Symbol> traverseInheritanceHierarchy(Symbol symbol, Symbol.ClassSymbol classSymbol) {
        List<Symbol> result = new ArrayList<>();

        final var superTypes = types.directSupertypes(classSymbol.type);
        for (Type superType : superTypes) {
            if (superType == null || superType.tsym == null) {
                continue;
            }
            result.addAll(traverseInheritanceHierarchy(symbol, (Symbol.ClassSymbol) superType.tsym));
        }

        final var first = classSymbol.members().findFirst(symbol.name);
        if (first == null) {
            return result;
        }
        for (Symbol sym : classSymbol.members().getSymbolsByName(symbol.name)) {
            result.add(sym);
        }

        return result;
    }

    public void traverseClassSymbol(Symbol.ClassSymbol classSymbol) {
        if (!shouldBeGenerated(classSymbol)) {
            return;
        }

        clashingClasses
                .computeIfAbsent(getFlatNameWithoutPackage(classSymbol).toString(), k -> new ArrayList<>())
                .add(classSymbol);

        if (classSymbol.members() != null) {
            renameCangjieClashes(classSymbol.members());
        }

        final var depth = traversalPathMap.get(classSymbol);
        final var depthNext = (limitedDepth && depth != null) ? depth + 1 : null;

        final List<Symbol> members = getMembers(classSymbol);

        if (limitedDepth) {
            final var mergeDefaultMethods = defaultMethods.mergeMultipleInterfaceDefaultMethods(classSymbol);
            mergeDefaultMethods.removeIf(members::contains);

            for (Symbol.MethodSymbol methodSymbol : mergeDefaultMethods) {
                if (hasExceededMaxDepthPath(methodSymbol, depth)) {
                    continue;
                }
                members.add(methodSymbol);
            }
        }

        for (Symbol element : members) {
            if (element instanceof Symbol.MethodSymbol symbol) {
                for (var param : symbol.params()) {
                    putToQueue(erasureType(param.type, types), depthNext);
                }
                if (!symbol.isConstructor()) {
                    putToQueue(erasureType(symbol.getReturnType(), types), depthNext);
                }
            } else if (element instanceof Symbol.VarSymbol symbol) {
                putToQueue(erasureType(symbol.type, types), depthNext);
            }
        }
        final var superTypes = collectSuperTypes(classSymbol, types);
        for (Type superType : superTypes) {
            putToQueue(superType.tsym, depthNext);
        }
    }

    public void computeSymbolsToMangle() {
        if (!onePackageMode) {
            return;
        }
        for (final var entry : clashingClasses.entrySet()) {
            final var value = entry.getValue();

            // Clashes with std.core entities should be mangled as well
            final var shortClassName = getFlatNameWithoutPackage(value.get(0)).toString();
            boolean shouldBeMangled = (value.size() == 1)
                    && importsConfig != null
                    && STD_CORE_NAMES.contains(shortClassName);

            if (value.size() <= 1 && !shouldBeMangled) {
                if (value.size() == 1) {
                    boolean canCaseClash = clashingClasses
                            .keySet()
                            .stream()
                            .filter(c -> c.equalsIgnoreCase(entry.getKey()))
                            .count() > 1;
                    if (canCaseClash) {
                        clashingFiles.add(entry.getKey());
                    }
                }
                continue;
            }

            System.err.println("Clashing class name is `" + entry.getKey() + "`");
            for (final var name : value) {
                addSymbolsToMangle(name);
            }
        }
        System.err.println("");
    }

    public void traverseDependenciesClosure(Symbol.ClassSymbol classSymbol) {
        if (classSymbol != null) {
            if (!shouldBeGenerated(classSymbol)) {
                String message = "Class " + classSymbol.flatname
                        + "has inappropriate access modifier so it is skipped for mirrors generation.";
                System.out.println(message);
                return;
            }
            if (limitedDepth && traversalPathMap.get(classSymbol) == null) {
                traversalPathMap.put(classSymbol, 0);
            }
            queue.add(classSymbol);
        }
        while (!queue.isEmpty()) {
            final var curSymbol = queue.remove();
            if (visited.add(curSymbol)) {
                traverseClassSymbol(curSymbol);
            }
        }
    }

    public void generateMirrorsWithDependencies() {
        FileWriter outputStream = null;
        try {
            if (importsConfig != null) {
                outputStream = new FileWriter(defaultImportsConfig, true);
            }

            for (Symbol.ClassSymbol curSymbol : visited) {
                if (curSymbol == symtab.objectType.tsym || curSymbol == symtab.stringType.tsym) {
                    continue;
                }

                currentClass = curSymbol;
                try {
                    generateMirror(curSymbol);
                } finally {
                    assert currentClass == curSymbol;
                    currentClass = null;
                }

                if (importsConfig != null) {
                    if (importsMap.containsKey(curSymbol.flatname.toString())) {
                        continue;
                    }
                    String identifier = getSymbolsToMangle().contains(curSymbol)
                            ? mangleClassName(curSymbol)
                            : addUnderscoresIfNeeded(getFlatNameWithoutPackage(curSymbol));
                    String qualifiedName = QualifiedName.get(userPackageName, identifier).toString();
                    outputStream.write(curSymbol.flatname.toString() + " " + qualifiedName + "\n");
                }
            }
        } catch (IOException e) {
            System.out.print(e.getMessage());
        } finally {
            if (outputStream != null) {
                try {
                    outputStream.close();
                } catch (IOException e) {
                    System.out.print(e.getMessage());
                }
            }
            newName.clear();
        }
    }
}
