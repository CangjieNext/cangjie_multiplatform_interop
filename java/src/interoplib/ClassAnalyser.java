// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package cangjie.interop.java;

import java.util.HashMap;

/**
 * This class provides internal API used by the synthetic glue code.
 * It provides the facility to calculate the bitmask that enables
 * fast virtual method calls.
 *
 * @since 2025-10-20
 */
public final class ClassAnalyser<T> {
    private final Class<T> baseClass;
    private final MethodDef[] virtualMethods;

    private volatile HashMap<Class<?>, Long> classMap = new HashMap<>();

    /**
     * Create the specialized analyzer instance.
     * 
     * @param baseClass super class descriptor
     * @param virtualMethods virtual method definitions to analyze
     */
    public ClassAnalyser(Class<T> baseClass, MethodDef[] virtualMethods) {
        this.baseClass = baseClass;
        this.virtualMethods = virtualMethods;
        if (baseClass.isInterface()) {
            throw new IllegalArgumentException();
        }
        for (MethodDef mdef : virtualMethods) {
            try {
                baseClass.getDeclaredMethod(mdef.name, mdef.parameters);
            } catch (NoSuchMethodException e) {
                throw new IllegalArgumentException(e.getMessage());
            }
        }
    }

    /**
     * Perform the actual analysis and calculate the required bitmask.
     * 
     * @param clazz the child class to compute the mask for
     * @return the bitmask with bits set for each method overridden in any super class
     */
    public <E extends T> long getOverrideMask(Class<E> clazz) {
        if (clazz == baseClass) {
            return 0;
        }
        if (!baseClass.isAssignableFrom(clazz)) {
            throw new IllegalArgumentException("Class " + clazz + " should inherit " + baseClass);
        }
        try {
            if (!getPackageName(clazz).equals(getPackageName(baseClass))) {
                clazz.getDeclaredMethod("finalize");
                throw new IllegalArgumentException("Class" + clazz + " is not allowed to have finalizer");
            }
        } catch (NoSuchMethodException e) {
            // Normal behavior (no finalizer overridden)
        }
        Long overrideMask = classMap.get(clazz);
        if (overrideMask != null) {
            return overrideMask;
        }
        long mask = 0L;
        for (Class<?> c = clazz; c != baseClass; c = c.getSuperclass()) {
            for (int i = 0; i < virtualMethods.length && i < 64; i++) {
                try {
                    clazz.getDeclaredMethod(virtualMethods[i].name, virtualMethods[i].parameters);
                    mask |= 1L << i;
                } catch (NoSuchMethodException e) {
                    // Normal behavior (method not overridden)
                }
            }
        }
        synchronized (this) {
            if (classMap.containsKey(clazz)) {
                return mask;
            }
            HashMap<Class<?>, Long> newMap = new HashMap<>(classMap);
            newMap.put(clazz, mask);
            classMap = newMap;
        }
        return mask;
    }

    private String getPackageName(Class<?> clazz) {
        Package pkg = clazz.getPackage();
        return pkg != null ? pkg.getName() : "";
    }
}