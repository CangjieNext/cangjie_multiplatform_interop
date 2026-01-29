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

import vendor.com.sun.tools.javac.code.Flags;
import vendor.com.sun.tools.javac.code.Kinds;
import vendor.com.sun.tools.javac.code.Scope;
import vendor.com.sun.tools.javac.code.Symbol;
import vendor.com.sun.tools.javac.code.Symtab;
import vendor.com.sun.tools.javac.code.Type;
import vendor.com.sun.tools.javac.code.Types;
import vendor.com.sun.tools.javac.util.Context;
import vendor.javax.lang.model.element.Modifier;

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Deque;
import java.util.List;

import static cangjie.interop.driver.VisitorUtils.shouldBeGenerated;
import static cangjie.interop.driver.VisitorUtils.hasOnlyAppropriateDeps;
import static cangjie.interop.driver.VisitorUtils.isPackagePrivate;

public final class OverrideChains {
    private final Types types;
    private final Symtab symtab;

    private OverrideChains(Context context) {
        context.put(OverrideChains.class, this);
        types = Types.instance(context);
        symtab = Symtab.instance(context);
    }

    public static OverrideChains instance(Context context) {
        final var instance = context.get(OverrideChains.class);
        return instance == null ? new OverrideChains(context) : instance;
    }

    public boolean overridesWithFilter(Symbol childMethod, Symbol superMethod) {
        if (!hasOnlyAppropriateDeps(childMethod, types)
                || !hasOnlyAppropriateDeps(superMethod, types)) {
            return false;
        }
        return overrides(childMethod, superMethod);
    }

    /**
     * More convenient version of {@link Symbol#overrides(Symbol, Symbol.TypeSymbol, Types, boolean)}.
     */
    public boolean overrides(Symbol childMethod, Symbol superMethod) {
        // This operator is reflexive for convenience.
        if (childMethod == superMethod) {
            return true;
        }

        if (childMethod.name != superMethod.name) {
            return false;
        }

        if (childMethod.kind != Kinds.Kind.MTH || superMethod.kind != Kinds.Kind.MTH) {
            // Only methods can participate in overriding.
            return false;
        }

        if (childMethod.isStatic() || superMethod.isStatic()) {
            // Static methods do not participate in overriding (they only do hiding).
            return false;
        }

        final var classSymbol = superMethod.enclClass();
        return childMethod.overrides(superMethod, classSymbol, types, true);
    }

    public boolean overridesNonAbstractMethod(Symbol.MethodSymbol symbolImpl) {
        assert symbolImpl.getModifiers().contains(Modifier.ABSTRACT) : symbolImpl;

        final var chains = buildMethodOverrideChains(symbolImpl, (Symbol.TypeSymbol) symbolImpl.owner);
        assert !chains.isEmpty() : symbolImpl;

        for (final var chain : chains) {
            for (final var symbols : chain) {
                // Covariant return types will use signature of the super type, or
                // equivalently, that of the bridge method.
                // Abstract modifier will be decided by the implementation method though.
                for (final var symbol : symbols) {
                    if ((symbol.flags() & Flags.BRIDGE) != 0) {
                        // Bridges have implementation by definition, but they will be invisible in the mirrors.
                        continue;
                    }

                    // N.B.: Modifiers also take default modifier into account, unlike flags.
                    if (!symbol.getModifiers().contains(Modifier.ABSTRACT)
                            && shouldBeGenerated(symbol.owner)) {
                        return true;
                    }
                }
            }
        }

        return false;
    }

    public boolean isMethodOverriddenInClass(Symbol.ClassSymbol childClass, Symbol superMethod) {
        for (final var childMethod : childClass.members().getSymbolsByName(superMethod.name)) {
            if (overridesWithFilter(childMethod, superMethod)) {
                return true;
            }
        }

        return false;
    }

    private List<Symbol.MethodSymbol> findOverridingMethod(Symbol.MethodSymbol childMethod,
                                                           Symbol.TypeSymbol classSymbol) {
        List<Symbol.MethodSymbol> result = null;
        for (final var superSymbol : classSymbol.members().getSymbolsByName(childMethod.name)) {
            if (!(superSymbol instanceof Symbol.MethodSymbol superMethod)) {
                continue;
            }

            if (overrides(childMethod, superMethod)) {
                if (result == null) {
                    result = new ArrayList<>();
                }

                result.add(superMethod);
            }
        }

        return result;
    }

    public List<Deque<List<Symbol.MethodSymbol>>> buildMethodOverrideChains(Symbol.MethodSymbol childMethod,
                                                                            Symbol.TypeSymbol currentClass) {
        final var currentMethods = findOverridingMethod(childMethod, currentClass);
        // It is a good idea to update the `childMethod` to be closer to `currentClass`, when possible.
        final var currentOrChildMethod = currentMethods != null && currentMethods.size() == 1
                ? currentMethods.get(0)
                : childMethod;
        assert currentOrChildMethod != null;

        List<Deque<List<Symbol.MethodSymbol>>> result = null;
        final var superTypes = types.directSupertypes(currentClass.type);
        final var skipObject = currentClass.isInterface() && superTypes.size() != 1;
        for (final var superType : superTypes) {
            if (superType == null) {
                continue;
            }

            final var superTypeSymbol = superType.tsym;
            if (superTypeSymbol == null) {
                continue;
            }

            if (skipObject && superTypeSymbol == symtab.objectType.tsym) {
                // If the interface is non-leaf (extends other interfaces),
                // we ignore the implicit Object supertype, to make sure that
                // the "root" property of the method can be correctly inferred from the list of chains.
                continue;
            }

            final var superChains = buildMethodOverrideChains(currentOrChildMethod, superTypeSymbol);
            if (superChains.isEmpty()) {
                continue;
            }

            if (result == null) {
                result = new ArrayList<>();
            }

            result.addAll(superChains);
        }

        if (currentMethods != null) {
            if (result == null) {
                result = new ArrayList<>();
            }
            if (result.isEmpty()) {
                result.add(new ArrayDeque<>());
            }

            // Prepend the overriding methods to all chains
            for (final var chain : result) {
                chain.addFirst(currentMethods);
            }
        }

        return result != null ? result : List.of();
    }

    public Symbol.MethodSymbol findRootMethod(Symbol.MethodSymbol method, Symbol.ClassSymbol classSymbol) {
        return findRootMethod(method, classSymbol, true);
    }

    public Symbol.MethodSymbol findRootMethod(Symbol.MethodSymbol method, Symbol.ClassSymbol classSymbol,
                                              boolean considerModifiers) {
        final var chains = buildMethodOverrideChains(method, classSymbol);
        assert !chains.isEmpty() : method + " | " + classSymbol;
        // Get any root method
        if (!considerModifiers) {
            return chains.get(0).getLast().get(0);
        }
        for (var chain : chains) {
            List<Symbol.MethodSymbol> last = chain.getLast();
            Symbol.MethodSymbol superMethod = last.stream().filter(sym ->
                    hasOnlyAppropriateDeps(sym, types)
                            && shouldBeGenerated(sym.owner)).findFirst().orElse(null);
            if (superMethod != null) {
                return superMethod;
            }
        }
        return method;
    }

    public Symbol.MethodSymbol getImplementationOfSuper(Symbol.MethodSymbol method, Scope classScope) {
        Symbol.MethodSymbol result = method;
        for (Symbol symbol : classScope.getSymbolsByName(method.name)) {
            if (symbol instanceof Symbol.MethodSymbol methodSymbol) {
                if (result == methodSymbol) {
                    continue;
                }
                if (overridesWithFilter(result, methodSymbol)) {
                    result = methodSymbol;
                }
            }
        }
        return result;
    }

    private boolean isRenamedInChain(Symbol.MethodSymbol methodSymbol,
                                     Deque<List<Symbol.MethodSymbol>> chain, boolean inInterface) {
        // @ForeignName can only be present on the root definition.
        // Figure out if `methodSymbol` is the root definition in the given chain.
        if (inInterface && chain.size() >= 2) {
            // Special case for interfaces: they can override methods from Object,
            // but the FE still requires @ForeignName annotation on them.
            // Consider such interface methods as root.
            final var it = chain.descendingIterator();
            final var lastObject = it.next();
            final var lastInterface = it.next();
            for (final var rootMethod : lastObject) {
                if (rootMethod.owner != symtab.objectType.tsym) {
                    continue;
                }

                for (final var interfaceMethod : lastInterface) {
                    if (!interfaceMethod.owner.isInterface()) {
                        continue;
                    }

                    if (interfaceMethod == methodSymbol) {
                        // @ForeignName can only be present on the root definition.
                        return true;
                    }
                }
            }
        }

        for (final var rootMethod : chain.getLast()) {
            // All methods at the end of each override chain are root.
            if (rootMethod == methodSymbol
                    && shouldBeGenerated(rootMethod.owner)
                    && hasOnlyAppropriateDeps(rootMethod, types)) {
                return true;
            }
        }

        return false;
    }

    public boolean isRenamedInThisClass(Symbol.ClassSymbol classSymbol, Symbol.MethodSymbol methodSymbol) {
        final var inInterface = classSymbol.isInterface();

        final var chains = buildMethodOverrideChains(methodSymbol, classSymbol);
        assert !chains.isEmpty() : methodSymbol;

        for (final var chain : chains) {
            if (isRenamedInChain(methodSymbol, chain, inInterface)) {
                return true;
            }
        }

        return false;
    }

    public boolean isPackagePrivateOverridden(Symbol.MethodSymbol childMethod, Symbol.ClassSymbol classSymbol) {
        Symbol.MethodSymbol result = childMethod;
        List<Type> closure = types.closure(classSymbol.type);
        for (Type type : closure) {
            result = getImplementationOfSuper(result, type.tsym.members());
        }
        return isPackagePrivate(result.owner);
    }
}
