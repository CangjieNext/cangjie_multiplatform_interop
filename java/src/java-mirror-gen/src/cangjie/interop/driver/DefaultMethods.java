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
import vendor.com.sun.tools.javac.code.Symbol;
import vendor.com.sun.tools.javac.code.Types;
import vendor.com.sun.tools.javac.util.Context;

import java.util.ArrayList;
import java.util.Collection;
import java.util.HashSet;
import java.util.LinkedHashSet;
import java.util.List;

import static cangjie.interop.driver.VisitorUtils.collectSuperInterfaces;
import static cangjie.interop.driver.VisitorUtils.shouldBeGenerated;

public final class DefaultMethods {
    private final Types types;
    private final OverrideChains overrideChains;

    private DefaultMethods(Context context) {
        context.put(DefaultMethods.class, this);
        types = Types.instance(context);
        overrideChains = OverrideChains.instance(context);
    }

    public static DefaultMethods instance(Context context) {
        final var instance = context.get(DefaultMethods.class);
        return instance == null ? new DefaultMethods(context) : instance;
    }

    private List<Symbol.MethodSymbol> getAllDefaultMethods(Symbol.ClassSymbol superInterface) {
        final var result = new ArrayList<Symbol.MethodSymbol>();

        final var interfaces = collectSuperInterfaces(superInterface, types);
        for (var superType : interfaces) {
            if (superType == null || superType.tsym == null) {
                continue;
            }

            for (var methodSymbol : getAllDefaultMethods((Symbol.ClassSymbol) superType.tsym)) {
                if (result.stream().anyMatch(s -> overrideChains.overridesWithFilter(methodSymbol, s)
                        && shouldBeGenerated(s.owner))) {
                    continue;
                }

                result.add(methodSymbol);
            }
        }

        for (var sym : superInterface.members().getSymbols()) {
            if (!(sym instanceof Symbol.MethodSymbol methodSymbol)) {
                continue;
            }

            if (result.contains(methodSymbol)) {
                continue;
            }

            if (result.stream().anyMatch(s -> overrideChains.overridesWithFilter(methodSymbol, s)
                    && shouldBeGenerated(s.owner))) {
                continue;
            }

            if (methodSymbol.isDefault() || methodSymbol.isStatic()) {
                result.add(methodSymbol);
            }
        }

        return result;
    }

    public Collection<Symbol.MethodSymbol> mergeMultipleInterfaceDefaultMethods(Symbol.ClassSymbol childClass) {
        final var result = new LinkedHashSet<Symbol.MethodSymbol>();
        final var visitedSymbols = new HashSet<Symbol.TypeSymbol>();

        final var interfaces = collectSuperInterfaces(childClass, types);
        for (final var leftInterface : interfaces) {
            final var leftMethodList = getAllDefaultMethods((Symbol.ClassSymbol) leftInterface.tsym);
            for (final var leftMethod : leftMethodList) {
                if (overrideChains.isMethodOverriddenInClass(childClass, leftMethod)) {
                    continue;
                }

                for (var rightInterface : interfaces) {
                    if (rightInterface == leftInterface) {
                        continue;
                    }

                    if (visitedSymbols.contains(rightInterface.tsym)) {
                        continue;
                    }

                    final var rightMethodList = getAllDefaultMethods((Symbol.ClassSymbol) rightInterface.tsym);
                    for (final var rightMethod : rightMethodList) {
                        if (leftMethod != rightMethod) {
                            continue;
                        }

                        // Exclude synthetic members of interfaces like java.lang.reflect.AnnotatedElement
                        // lambda$getDeclaredAnnotationsByType$0
                        if ((leftMethod.flags() & Flags.SYNTHETIC) != 0) {
                            continue;
                        }

                        result.add(leftMethod);
                    }
                }
            }

            visitedSymbols.add(leftInterface.tsym);
        }

        return result;
    }

    public Collection<Symbol.MethodSymbol> findAllDefaultMethods(Symbol.ClassSymbol childClass) {
        List<Symbol.MethodSymbol> result = null;

        final var interfaces = collectSuperInterfaces(childClass, types);
        for (var anInterface : interfaces) {
            final var methodList =
                    getAllDefaultMethods((Symbol.ClassSymbol) anInterface.tsym);

            methods:
            for (final var method : methodList) {
                if (!method.isDefault()) {
                    continue;
                }

                if (overrideChains.isMethodOverriddenInClass(childClass, method)) {
                    continue;
                }

                if (result == null) {
                    result = new ArrayList<>();
                } else {
                    final var methodErasure = types.erasure(method.type);
                    for (final var existing : result) {
                        if (existing.name != method.name) {
                            continue;
                        }

                        if (types.hasSameArgs(methodErasure, types.erasure(existing.type))) {
                            continue methods;
                        }
                    }
                }

                result.add(method);
            }
        }

        if (result == null) {
            return List.of();
        }

        for (final var type : types.closure(childClass.type)) {
            if (type.tsym instanceof Symbol.ClassSymbol csym &&
                    csym != childClass && !csym.isInterface() && shouldBeGenerated(csym)) {
                final var superMethods = findAllDefaultMethods(csym);
                if (superMethods.isEmpty()) {
                    continue;
                }

                result.removeIf(child -> superMethods.stream()
                        .anyMatch(base -> overrideChains.overridesWithFilter(child, base)));
            }
        }

        return result;
    }
}
