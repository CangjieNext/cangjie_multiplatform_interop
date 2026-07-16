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
import static cangjie.interop.driver.VisitorUtils.addSymbolsToMangle;
import static cangjie.interop.driver.VisitorUtils.collectImports;
import static cangjie.interop.driver.VisitorUtils.collectSuperTypes;
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
import static cangjie.interop.driver.VisitorUtils.nameTypeVar;
import static cangjie.interop.driver.VisitorUtils.setImportsMap;
import static cangjie.interop.driver.VisitorUtils.setNames;
import static cangjie.interop.driver.VisitorUtils.setSymtab;
import static cangjie.interop.driver.VisitorUtils.shouldBeGenerated;
import static cangjie.interop.util.StdCoreNames.STD_CORE_NAMES;
import static vendor.com.sun.tools.javac.code.Kinds.Kind.VAR;
import static vendor.com.sun.tools.javac.util.Assert.check;
import static vendor.com.sun.tools.javac.util.Assert.checkNonNull;
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
import vendor.com.sun.tools.javac.code.ModuleFinder;
import vendor.com.sun.tools.javac.comp.Modules;
import vendor.com.sun.tools.javac.util.Context;
import vendor.com.sun.tools.javac.util.Log;
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

/**
 * The actual implementation of Cangjie mirror generation.
 *
 * @since 2025-01-01
 */
public final class EmitMirrorVisitor {
    private final Queue<Symbol.ClassSymbol> queue;
    private final Set<Symbol.ClassSymbol> visited;
    private final HashMap<Symbol.ClassSymbol, Integer> traversalPathMap;
    private final JavaFileManager fileManager;
    private final Types types;
    private final Symtab symtab;
    private final Modules modules;
    private final ModuleFinder moduleFinder;
    private final Log log;
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
        modules = Modules.instance(context);
        moduleFinder = ModuleFinder.instance(context);
        log = Log.instance(context);
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
                log.printRawLines(e.getMessage());
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
                    readImportsMapFile(br);
                } catch (IOException e) {
                    log.printRawLines("Error while reading a file.");
                }

                setImportsMap(importsMap);
            }
        }
    }

    private void readImportsMapFile(BufferedReader br) throws IOException {
        String str;
        while ((str = br.readLine()) != null) {
            String[] parts = str.split(" ");
            if (parts.length != 2) {
                continue;
            }
            importsMap.put(parts[0], parts[1]);
        }
    }

    /**
     * Get or create the singleton instance, bound to the given context.
     *
     * @param context compilation session context
     * @return the singleton instance
     */
    public static EmitMirrorVisitor instance(Context context) {
        final var instance = context.get(EmitMirrorVisitor.class);
        return instance == null ? new EmitMirrorVisitor(context) : instance;
    }

    private void initialize(Collection<Symbol.ClassSymbol> classSymbols) {
        if (!limitedDepth) {
            return;
        }

        for (final var classSymbol : classSymbols) {
            traversalPathMap.put(classSymbol, 0);
        }
    }

    private static cangjie.interop.cangjie.sema.TypeKind kind(Symbol.ClassSymbol sym) {
        return switch (sym.getKind()) {
            case CLASS, ENUM, RECORD -> cangjie.interop.cangjie.sema.TypeKind.CLASS;
            case INTERFACE -> cangjie.interop.cangjie.sema.TypeKind.INTERFACE;
            case ANNOTATION_TYPE -> cangjie.interop.cangjie.sema.TypeKind.ANNOTATION;
            default -> throw new RuntimeException("Not implemented: " + sym.getKind());
        };
    }

    private void warnAboutOverwrite() {
        for (final var entry : writtenPaths.entrySet()) {
            final var value = entry.getValue();
            if (value.size() <= 1) {
                continue;
            }

            log.printRawLines("Multiple classes write to the same file `" + entry.getKey() + "`:");
            for (final var name : value) {
                log.printRawLines("* " + name);
            }
        }
    }

    private CJTree.Declaration translate(Symbol.MethodSymbol symbol) {
        if (symbol.owner.hasOuterInstance()) {
            // to generate arg in constructor for enclosing class
            symbol.flags_field = symbol.flags_field | Flags.SYNTHETIC;
        }
        var symbolImpl = symbol.implementation((Symbol.ClassSymbol) symbol.owner, types, true);
        if (symbolImpl == null) {
            symbolImpl = symbol;
        }

        return new MethodTranslation(symbol, symbolImpl).translate();
    }

    private void translateTypeParameters(
            List<Symbol.TypeVariableSymbol> jTypeParameters,
            CJTree.Declaration.TypeParameterOwnerDeclaration owner) {
        for (final var jTypeParameter : jTypeParameters) {
            final var name = nameTypeVar(jTypeParameter);
            owner.addTypeParameter(name);
            final var boundsAsNames = jTypeParameter.getBounds().stream()
                    .filter(bound -> bound.tsym != symtab.objectType.tsym)
                    .map(VisitorUtils::name)
                    .toList();
            if (!boundsAsNames.isEmpty()) {
                final var constraint = new CJTree.GenericConstraint(name);
                constraint.bounds.addAll(boundsAsNames);
                owner.addConstraint(constraint);
            }
        }
    }

    private final class MethodTranslation {
        private final Symbol.MethodSymbol symbol;
        private final Symbol.MethodSymbol symbolImpl;
        private final CJTree.Declaration.FunctionDeclaration methodDecl;
        private final boolean isSynthetic;
        private final boolean isRedundantModifier;
        private final Set<Modifier> javaModifiers;
        private final ModifierList cjModifiers;

        MethodTranslation(Symbol.MethodSymbol symbol, Symbol.MethodSymbol symbolImpl) {
            this.symbol = symbol;
            this.symbolImpl = symbolImpl;

            final var newMethodName = newName.get(symbol);
            final var methodName = newMethodName != null ? newMethodName.toString() : scope.generateMethodName(symbol);
            methodDecl = new CJTree.Declaration.FunctionDeclaration(methodName, symbol.isConstructor());

            isSynthetic = symbol.owner != currentClass;
            isRedundantModifier = currentClass.isInterface();
            javaModifiers = symbolImpl.getModifiers();
            cjModifiers = new ModifierList();
        }

        CJTree.Declaration.FunctionDeclaration translate() {
            translateMethodParameters();
            translateTypeParameters(symbol.getTypeParameters(), methodDecl);
            computeMethodModifiers();
            if (!symbol.isConstructor()) {
                adjustNonConstructor();
            }
            methodDecl.modifiers.addAll(cjModifiers);
            return methodDecl;
        }

        private void translateMethodParameters() {
            final var infoList = ParameterInfo.makeParameters(scope.getParameters(symbol));

            int i = 0;
            for (var param : infoList) {
                final var decl = new CJTree.Declaration.VariableDeclaration.Parameter(param.name());

                if (i == 0 && param.isNameSynthetic() && symbol.isConstructor()) {
                    final var classSymbol = symbol.owner;
                    final var outerThisType = types.erasure(classSymbol.type.getEnclosingType());
                    decl.setType(name(outerThisType, true));
                } else {
                    Symbol.VarSymbol jParamSymbol = symbol.params().get(i);
                    final var hasNotNullAttribute = considerNotNullAnnotations
                            && (hasNotNullAnnotation(jParamSymbol)
                            || hasNotNullSigParamAnnotation(symbol, TargetType.METHOD_FORMAL_PARAMETER, i));
                    final var paramType = jParamSymbol.type;
                    if (paramType.tsym instanceof Symbol.TypeVariableSymbol typeVariableSymbol) {
                        final var freeTypeVar = new CJTree.Expression.Name.SimpleName.ErasedTypeVariableName(
                                nameTypeVar(typeVariableSymbol), name(types.erasure(paramType))
                        );
                        decl.setType(freeTypeVar);
                    } else {
                        decl.setType(name(paramType, hasNotNullAttribute));
                    }
                    i++;
                }
                methodDecl.valueParameters.add(decl);
            }
        }

        private void computeMethodModifiers() {
            // Let synthetic methods be those that don't exist in the .class file.
            // They will not belong to the current class - they come from its super types.
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
        }

        private void adjustNonConstructor() {
            if (javaModifiers.contains(Modifier.STATIC)) {
                cjModifiers.add(Modifiers.STATIC.toCJTree());
            } else {
                if (!symbol.enclClass().isFinal() && !javaModifiers.contains(Modifier.FINAL)
                        && !isRedundantModifier) {
                    cjModifiers.add(Modifiers.OPEN.toCJTree());
                }
            }

            final var returnType = symbol.getReturnType();
            final var hasNotNullAttribute = considerNotNullAnnotations
                    && (hasNotNullAnnotation(symbol)
                    || hasNotNullTypeAnnotation(symbol, TargetType.METHOD_RETURN));
            if (returnType.tsym instanceof Symbol.TypeVariableSymbol typeVariableSymbol) {
                final var freeTypeVar = new CJTree.Expression.Name.SimpleName.ErasedTypeVariableName(
                        nameTypeVar(typeVariableSymbol), name(types.erasure(returnType))
                );
                methodDecl.setReturnType(freeTypeVar);
            } else {
                methodDecl.setReturnType(name(returnType, hasNotNullAttribute));
            }

            final var isToString = Utils.isToString(symbol);
            if (isToString
                    && methodDecl.getReturnType() instanceof
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
    }

    private boolean methodStillNotImplemented(Symbol.MethodSymbol superMethod) {
        check(superMethod.getModifiers().contains(Modifier.ABSTRACT), superMethod);

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

    private CJTree.Declaration translate(Symbol.VarSymbol varSymbol) {
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
        final var type = varSymbol.type;
        final var hasNotNullAttribute = !generateDefinition
                && (hasNotNullAnnotation(varSymbol)
                    || hasNotNullTypeAnnotation(varSymbol, TargetType.FIELD));
        if (type.tsym instanceof Symbol.TypeVariableSymbol typeVariableSymbol) {
            final var freeTypeVar = new CJTree.Expression.Name.SimpleName.ErasedTypeVariableName(
                    nameTypeVar(typeVariableSymbol), name(types.erasure(type))
            );
            decl.setType(freeTypeVar);
        } else {
            decl.setType(name(type, hasNotNullAttribute));
        }
        if (generateDefinition) {
            decl.setInitializer(defaultValueForType(type));
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
            if (limitedDepth) {
                final var depth = traversalPathMap.get(paramSymbol);
                if (depth == null) {
                    traversalPathMap.put(paramSymbol, pathLevel);
                    return;
                }
                if (depth > pathLevel) {
                    traversalPathMap.put(paramSymbol, pathLevel);
                    visited.remove(paramSymbol);
                }
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
        if (!limitedDepth || depth == null) {
            return false;
        }

        if (symbol instanceof Symbol.MethodSymbol methodSymbol) {
            if (methodSymbol.owner.isInterface() && !methodSymbol.isDefault() && !methodSymbol.isStatic()) {
                if (anyImplementorWithoutImplementation(methodSymbol)) {
                    return true;
                }
            }
        }

        if (depth == maxPathLength) {
            if (symbol instanceof Symbol.MethodSymbol methodSymbol) {
                return methodSignatureExceedsMaxDepth(methodSymbol);
            }
            if (symbol instanceof Symbol.VarSymbol varSymbol) {
                return exceedMaxDepth(erasureType(varSymbol.type, types));
            }
        }

        return false;
    }

    private boolean methodSignatureExceedsMaxDepth(Symbol.MethodSymbol methodSymbol) {
        for (var param : methodSymbol.params()) {
            final var paramTypeSymbol = erasureType(param.type, types);
            if (exceedMaxDepth(paramTypeSymbol)) {
                return true;
            }
        }
        final var returnTypeSymbol = erasureType(methodSymbol.getReturnType(), types);
        return exceedMaxDepth(returnTypeSymbol);
    }

    private boolean anyImplementorWithoutImplementation(Symbol.MethodSymbol methodSymbol) {
        for (Symbol.ClassSymbol classSymbol : traversalPathMap.keySet()) {
            if (classSymbol.isAbstract() || classSymbol.isInterface()) {
                continue;
            }

            final var closure = types.closure(classSymbol.type);
            for (Type type : closure) {
                if (!type.tsym.equals(methodSymbol.owner)) {
                    continue;
                }
                if (overrideChains.isMethodOverriddenInClass(classSymbol, methodSymbol)) {
                    continue;
                }

                return true;
            }
        }
        return false;
    }

    private final class MembersCollection {
        private final Symbol.ClassSymbol classSymbol;
        private final Integer depth;
        private final Scope.WriteableScope tmpScope;
        private final List<Symbol> members = new ArrayList<>();
        private final LinkedHashSet<Symbol.MethodSymbol> visitedMethods = new LinkedHashSet<>();

        MembersCollection(Symbol.ClassSymbol classSymbol) {
            this.classSymbol = classSymbol;
            this.depth = traversalPathMap.get(classSymbol);
            this.tmpScope = classSymbol.members();
        }

        List<Symbol> collect() {
            if (tmpScope == null) {
                return members;
            }

            for (Symbol element : tmpScope.getSymbols()) {
                if (!shouldBeGenerated(element)) {
                    continue;
                }
                if (element instanceof Symbol.MethodSymbol symbol) {
                    collectMethod(symbol);
                    continue;
                }
                if (element instanceof Symbol.VarSymbol varSymbol) {
                    collectVariable(varSymbol);
                }
            }

            Collections.reverse(members);
            return members;
        }

        private void collectMethod(Symbol.MethodSymbol symbol) {
            // JObject methods cause problems in interfaces. Skip them.
            if (interfaceObjectMethodsWorkaround && classSymbol.isInterface()
                    && types.overridesObjectMethod(symbol.enclClass(), symbol)) {
                return;
            }

            if (!hasOnlyAppropriateDeps(symbol, types)) {
                return;
            }

            final var methodImpl = overrideChains.findRootMethod(symbol, classSymbol);

            final var methodSymbol = symbol.isStatic()
                    ? methodImpl
                    : overrideChains.getImplementationOfSuper(symbol, tmpScope);

            check(methodSymbol == symbol || !symbol.isStatic(),
                    classSymbol + " : " + symbol + " | " + methodSymbol);

            if ((symbol.flags() & Flags.BRIDGE) != 0
                    && methodSymbol == symbol
                    && overrideChains.isPackagePrivateOverridden(methodSymbol, classSymbol)) {
                visitedMethods.add(methodSymbol);
                return;
            }

            if (hasExceededMaxDepthPath(methodSymbol, depth)) {
                return;
            }
            if (!hasOnlyAppropriateDeps(methodSymbol, types)) {
                return;
            }

            if (visitedMethods.add(methodSymbol)) {
                members.add(methodSymbol);
            }
        }

        private void collectVariable(Symbol.VarSymbol symbol) {
            if (symbol.owner.type.isInterface() && symbol.getModifiers().contains(Modifier.FINAL)) {
                return;
            }

            if (hasExceededMaxDepthPath(symbol, depth)) {
                return;
            }
            if (!hasOnlyAppropriateDeps(symbol, types)) {
                return;
            }
            members.add(symbol);
        }
    }

    private List<Symbol> getMembers(Symbol.ClassSymbol classSymbol) {
        return new MembersCollection(classSymbol).collect();
    }

    private final class ClassTranslation {
        private final Symbol.ClassSymbol classSymbol;
        private final CJTree.Declaration.TypeDeclaration classDecl;
        private final List<CJTree.Declaration> block = new ArrayList<>();
        private boolean hasInitWithoutParams = false;

        ClassTranslation(Symbol.ClassSymbol classSymbol) {
            this.classSymbol = classSymbol;
            this.classDecl = new CJTree.Declaration.TypeDeclaration(kind(classSymbol));
        }

        CJTree.Declaration.TypeDeclaration translateClass() {
            check(shouldBeGenerated(classSymbol), classSymbol);

            setType();
            translateTypeParameters(classSymbol.getTypeParameters(), classDecl);
            addSuperTypes();
            if (generateAnnotationMode) {
                addJavaMirrorAnnotation();
            }
            computeModifiers();
            visitBody();
            addSyntheticInit();
            classDecl.declarations.addAll(block);
            return classDecl;
        }

        private void setType() {
            final var name = getSymbolsToMangle().contains(classSymbol)
                    ? mangleClassName(classSymbol)
                    : addBackticksIfNeeded(getFlatNameWithoutPackage(classSymbol));

            classDecl.setType(new CJTree.Expression.Name.SimpleName.IdentifierName(name));
        }

        private void addSuperTypes() {
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
        }

        private void addJavaMirrorAnnotation() {
            final var javaMirrorAnnotation = new CJTree.Expression.Name.SimpleName.IdentifierName("JavaMirror");
            final var annotation = new CJTree.Declaration.Annotation(javaMirrorAnnotation);
            final var argument = new CJTree.Expression.Literal.String(formTypeName(classSymbol).toString());
            annotation.argumentList.add(argument);
            classDecl.annotations.add(annotation);
        }

        private void computeModifiers() {
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
        }

        private void visitBody() {
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
                    translateMemberMethod(methodSymbol, renamed, originalName);
                    continue;
                }
                if (element instanceof Symbol.VarSymbol varSymbol) {
                    translateMemberVariable(varSymbol, renamed, originalName);
                }
            }
        }

        private void translateMemberMethod(Symbol.MethodSymbol methodSymbol, boolean isRenamed, Name originalName) {
            if (!hasInitWithoutParams && methodSymbol.isConstructor()) {
                if (methodSymbol.params().isEmpty()) {
                    hasInitWithoutParams = true;
                }
            }

            final var tree = translate(methodSymbol);
            block.add(tree);

            if (generateAnnotationMode
                    && isRenamed
                    && overrideChains.isRenamedInThisClass(classSymbol, methodSymbol)) {
                tree.annotations.add(foreignNameAnnotationGen(originalName));
            }

            if (generateAnnotationMode
                    && currentClass.isInterface()
                    && (methodSymbol.isDefault() && !methodSymbol.isStatic()
                    || methodSymbol.isAbstract() && overrideChains.overridesNonAbstractMethod(methodSymbol))) {
                tree.annotations.add(defaultMethodAnnotationGen());
            }
        }

        private void translateMemberVariable(Symbol.VarSymbol varSymbol, boolean isRenamed, Name originalName) {
            final var tree = translate(varSymbol);
            block.add(tree);

            if (generateAnnotationMode && isRenamed) {
                tree.annotations.add(foreignNameAnnotationGen(originalName));
            }
        }

        private void addSyntheticInit() {
            if (classSymbol.isInterface() || !generateDefinition) {
                return;
            }
            if (hasInitWithoutParams && !classSymbol.hasOuterInstance()) {
                return;
            }

            final var initMethodTree = new CJTree.Declaration.FunctionDeclaration("init", true);
            initMethodTree.modifiers.add(Modifiers.PUBLIC.toCJTree());
            initMethodTree.setBody(new CJTree.Expression.Block(true));
            block.add(initMethodTree);
        }
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

    private void generateMirror(Symbol.ClassSymbol classSymbol) {
        final var ownerSymbol = findPackageSymbol(classSymbol);
        final var moduleName = userPackageName != null ? userPackageName : "UNNAMED";
        final var unitPkgName = !onePackageMode ? ownerSymbol.getQualifiedName().toString() : moduleName;

        final var unit = new CJTree.CompilationUnit();
        final var tree = new ClassTranslation(classSymbol).translateClass();

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
        checkNonNull(moduleName);
        path.append(moduleName).append('/').append("src/");

        if (!onePackageMode) {
            path.append(unitPkgName.replace('.', File.separatorChar)).append(File.separator);
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
            renameCangjieFieldClashes(symbols);
            renameCangjieMethodClashes(symbols);
        }
    }

    private void renameCangjieFieldClashes(List<Symbol> symbols) {
        final var fields = symbols.stream().filter(f -> f.kind == VAR).toList();
        if (fields.size() <= 1) {
            return;
        }

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

    private void renameCangjieMethodClashes(List<Symbol> symbols) {
        final var methods = new ArrayList<Symbol.MethodSymbol>();

        for (final var symbol : symbols) {
            if (symbol instanceof Symbol.MethodSymbol method) {
                methods.add(method);
                if (Utils.isHashCode(method)) {
                    newName.put(method, hashCode32);
                    continue;
                }
                if (Utils.isToString(method)) {
                    newName.put(method, toJString);
                }
            }
        }

        if (methods.size() <= 1) {
            return;
        }

        renameCangjieMethodClashesImpl(methods);
    }

    private void renameCangjieMethodClashesImpl(List<Symbol.MethodSymbol> methods) {
        final var allInstance = methods.stream().noneMatch(m -> m.isStatic());
        final var allStatic = methods.stream().allMatch(m -> m.isStatic());

        if (allInstance) {
            return;
        }
        for (var method : methods) {
            if (method.isStatic()) {
                final var newNameMethod = newName.get(method);
                boolean shouldBeMangledStatic = !allStatic
                        || newNameMethod != null
                        && newNameMethod.startsWith(method.name.append(staticSuffix));
                if (shouldBeMangledStatic) {
                    newName.put(method, method.name.append(staticSuffix));
                }
                // Cangjie prohibits redefinition of a static method with a different return type.
                // Rename the descendant method.
                final var hiddenSymbols = memberHiding.collectHiddenSymbols(method);

                if (hiddenSymbols.isEmpty()) {
                    continue;
                }

                if (hiddenSymbols.stream().noneMatch(s -> (s instanceof Symbol.MethodSymbol symbol)
                        && (!symbol.getReturnType().equals(method.getReturnType())))) {
                    continue;
                }

                if (!(method.getReturnType().tsym instanceof Symbol.ClassSymbol returnTypeSymbol)) {
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

    private void traverseClassSymbol(Symbol.ClassSymbol classSymbol) {
        if (!shouldBeGenerated(classSymbol)) {
            return;
        }

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
                continue;
            }
            if (element instanceof Symbol.VarSymbol symbol) {
                putToQueue(erasureType(symbol.type, types), depthNext);
            }
        }
        final var superTypes = collectSuperTypes(classSymbol, types);
        for (Type superType : superTypes) {
            putToQueue(superType.tsym, depthNext);
        }
    }

    private void computeSymbolsToMangle() {
        if (!onePackageMode) {
            return;
        }

        final Map<String, List<Symbol.ClassSymbol>> clashingClasses = new HashMap<>();
        for (final var classSymbol : visited) {
            clashingClasses
                    .computeIfAbsent(getFlatNameWithoutPackage(classSymbol).toString(), k -> new ArrayList<>())
                    .add(classSymbol);
        }
        for (final var entry : clashingClasses.entrySet()) {
            final var key = entry.getKey();
            final var value = entry.getValue();

            writeClashingClass(key, value, clashingClasses.keySet());
        }
    }

    private void writeClashingClass(String key, List<Symbol.ClassSymbol> value, Set<String> clashingClassNames) {
        // Clashes with std.core entities should be mangled as well
        final var shortClassName = getFlatNameWithoutPackage(value.get(0)).toString();
        boolean shouldBeMangled = (value.size() == 1)
                && importsConfig != null
                && STD_CORE_NAMES.contains(shortClassName);

        if (value.size() <= 1 && !shouldBeMangled) {
            if (value.size() == 1) {
                boolean canCaseClash = clashingClassNames
                        .stream()
                        .filter(c -> c.equalsIgnoreCase(key))
                        .count() > 1;
                if (canCaseClash) {
                    clashingFiles.add(key);
                }
            }
            return;
        }

        log.printRawLines("Clashing class name is `" + key + "`");
        for (final var name : value) {
            addSymbolsToMangle(name);
        }
    }

    private void traverseDependenciesClosure(Symbol.ClassSymbol classSymbol) {
        if (classSymbol != null) {
            if (!shouldBeGenerated(classSymbol)) {
                String message = "Class " + classSymbol.flatname
                        + "has inappropriate access modifier so it is skipped for mirrors generation.";
                log.printRawLines(message);
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

    private void generateMirrors() {
        FileWriter outputStream = null;
        try {
            if (importsConfig != null) {
                outputStream = new FileWriter(defaultImportsConfig, true);
            }

            for (Symbol.ClassSymbol curSymbol : visited) {
                generateMirrorsImpl(curSymbol, outputStream);
            }
        } catch (IOException e) {
            log.printRawLines(e.getMessage());
        } finally {
            if (outputStream != null) {
                try {
                    outputStream.close();
                } catch (IOException e) {
                    log.printRawLines(e.getMessage());
                }
            }
            newName.clear();
        }
    }

    private void generateMirrorsImpl(Symbol.ClassSymbol curSymbol, FileWriter importsOutputStream) throws IOException {
        if (curSymbol == symtab.objectType.tsym || curSymbol == symtab.stringType.tsym) {
            return;
        }
        if (curSymbol.classfile == null) {
            return;
        }
        if (curSymbol.hasOuterInstance() && curSymbol.isPrivate()) {
            return;
        }
        if (importsMap.containsKey(curSymbol.flatname.toString())) {
            return;
        }

        currentClass = curSymbol;
        try {
            generateMirror(curSymbol);
        } finally {
            check(currentClass == curSymbol);
            currentClass = null;
        }

        if (importsOutputStream == null) {
            return;
        }
        if (importsMap.containsKey(curSymbol.flatname.toString())) {
            return;
        }
        String identifier = getSymbolsToMangle().contains(curSymbol)
                ? mangleClassName(curSymbol)
                : addUnderscoresIfNeeded(getFlatNameWithoutPackage(curSymbol));
        String qualifiedName = QualifiedName.get(userPackageName, identifier).toString();
        importsOutputStream.write(curSymbol.flatname.toString() + " " + qualifiedName + "\n");
    }

    /**
     * Traverse all specified classes and generate mirrors for their transitive closure.
     *
     * @param classNames fully qualified root class names
     */
    public void traverseAndGenerate(Collection<String> classNames) {
        Collection<Symbol.ClassSymbol> classSymbols = composeClassSymbolsList(classNames);
        initialize(classSymbols);
        for (final var classSymbol : classSymbols) {
            traverseDependenciesClosure(classSymbol);
        }
        computeSymbolsToMangle();
        generateMirrors();
        warnAboutOverwrite();
    }

    private Collection<Symbol.ClassSymbol> composeClassSymbolsList(Collection<String> classNames) {
        Collection<Symbol.ClassSymbol> classSymbols = new ArrayList<>();
        for (final var name : classNames) {
            try {
                final var msym = modules.getDefaultModule();
                msym.complete();
                final var classSymbol = moduleFinder.classFinder.loadClass(msym, names.fromString(name));
                if (classSymbol != null && !classSymbol.isAnonymous()) {
                    classSymbols.add(classSymbol);
                }
            } catch (Exception ex) {
                ex.printStackTrace();
            }
        }
        return classSymbols;
    }
}
