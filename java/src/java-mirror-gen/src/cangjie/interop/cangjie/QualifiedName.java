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

package cangjie.interop.cangjie;

import static cangjie.interop.Utils.split;
import static vendor.com.sun.tools.javac.util.Assert.check;
import static vendor.com.sun.tools.javac.util.Assert.checkNonNull;

import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;
import java.util.Optional;

public final class QualifiedName {
    public static final String JAVA8 = "java8";
    public static final String UNNAMED = "UNNAMED";

    private static final Map<String, QualifiedName> intern = new HashMap<>();

    // These names will not require module and package qualification
    private static final Map<String, QualifiedName> names = new HashMap<>();

    private final String [] parts;
    private final String string;

    private QualifiedName(String [] parts, String string) {
        this.parts = parts;
        this.string = string;
        if (isFromStdCore()) {
            names.put(name(), this);
        }
    }

    public static QualifiedName get(String... parts) {
        checkNonNull(parts);
        check(parts.length > 0);
        final var sb = new StringBuilder(parts.length * 5);
        for (final var part : parts) {
            check(part != null, Arrays.toString(parts));
            check(part.strip().equals(part), Arrays.toString(parts));
            check(!part.isEmpty(), Arrays.toString(parts));
            check(part.indexOf('.') == -1, Arrays.toString(parts));
            check(part.indexOf('/') == -1, Arrays.toString(parts));
            check(part.indexOf('<') == -1, Arrays.toString(parts));
            check(part.indexOf('>') == -1, Arrays.toString(parts));
            if (!sb.isEmpty()) {
                sb.append('.');
            }
            sb.append(part);
        }
        final var string = sb.toString();
        return intern.computeIfAbsent(string, n -> new QualifiedName(parts, n));
    }

    public static Optional<QualifiedName> parse(String line) {
        if (line.isBlank()) {
            return Optional.empty();
        }

        final var found = findRootIdentifier(line);
        if (found != null) {
            return Optional.of(found);
        }

        return Optional.of(get(split(line.strip(), '.')));
    }

    static QualifiedName findRootIdentifier(String line) {
        return names.get(line);
    }

    @Override
    public String toString() {
        return string;
    }

    public String toStringNoModule() {
        return java.lang.String.join(".", parts(false, true));
    }

    public boolean equal(String str) {
        return string.equals(str);
    }

    public boolean isFromPackage(QualifiedName packageName) {
        return Arrays.equals(parts(true, false), packageName.parts);
    }

    public boolean isFromStdCore() {
        return "std".equals(moduleName()) && "core".equals(fullPackageName());
    }

    public boolean requiresImport() {
        if (isFromStdCore()) {
            return false;
        }

        return fullPackageName() != null;
    }

    public boolean isUnsupported() {
        return JAVA8.equals(moduleName());
    }

    public String generateImport() {
        check(requiresImport());
        final var builder = new StringBuilder(64);
        builder.append("import ");
        if (isUnsupported()) {
            builder.append("UNSUPPORTED").append('.').append(toStringNoModule());
        } else {
            builder.append(this);
        }
        return builder.toString();
    }

    public String moduleName() {
        return parts[0];
    }

    public String fullPackageName() {
        final var packageNames = packageNames();
        if (packageNames != null) {
            return java.lang.String.join(".", packageNames);
        }
        return null;
    }

    public String[] packageNames() {
        if (parts.length > 2) {
            return parts(false, false);
        }
        return null;
    }

    public String name() {
        return parts[parts.length - 1];
    }

    public String [] parts(boolean withModule, boolean withName) {
        return Arrays.copyOfRange(parts, withModule ? 0 : 1, parts.length - (withName ? 0 : 1));
    }

    public QualifiedName member(String name) {
        final var varParts = Arrays.copyOf(this.parts, this.parts.length + 1);
        varParts[varParts.length - 1] = name;
        return get(varParts);
    }
}
