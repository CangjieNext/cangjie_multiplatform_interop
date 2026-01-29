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

import vendor.com.sun.tools.javac.code.Flags;
import vendor.com.sun.tools.javac.code.Symbol;
import vendor.com.sun.tools.javac.code.Types;
import vendor.com.sun.tools.javac.util.Context;
import vendor.com.sun.tools.javac.util.Names;

import java.util.ArrayList;
import java.util.List;

public final class MapScope {
    private final Names names;
    private final Types types;

    private MapScope(Context context) {
        context.put(MapScope.class, this);
        names = Names.instance(context);
        types = Types.instance(context);
    }

    public static MapScope instance(Context context) {
        final var instance = context.get(MapScope.class);
        return instance == null ? new MapScope(context) : instance;
    }

    public List<?> getParameters(Symbol.MethodSymbol symbol) {
        final List<Object> result = new ArrayList<>();
        result.addAll(symbol.extraParams);
        result.addAll(symbol.getParameters());
        result.addAll(symbol.capturedLocals);

        // Workaround for JDK buggy behaviour for synthetic <init>(OuterClassType x0, AccessConstructorTag$1 x1)
        // x0 is missing from synthetic access constructor symbol extraParams.
        // Symbol.externalType(Types) hack is introduced in JDK instead of simply fixing the MethodSymbols.
        if (symbol.isConstructor()) {
            final var classSymbol = symbol.owner;

            if (classSymbol.hasOuterInstance()
                    && !classSymbol.isStatic()
                    && (symbol.flags() & Flags.SYNTHETIC) != 0) {
                final var outerThisType = types.erasure(classSymbol.type.getEnclosingType());
                result.add(0, outerThisType);
            }
        }

        return result;
    }

    public String generateMethodName(Symbol.MethodSymbol symbol) {
        if (symbol.name.equals(names.init)) {
            return "init";
        } else if (symbol.name.equals(names.clinit)) {
            return "clinit";
        } else {
            return addBackticksIfNeeded(symbol.name);
        }
    }
}
