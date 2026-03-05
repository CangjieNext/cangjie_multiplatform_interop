// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package cangjie.interop.java;

/**
 * This class provides internal API used by the synthetic glue code.
 * It represents a single virtual method, with its name
 * and partial signature.
 *
 * @since 2025-10-20
 */
public final class MethodDef {
    final String name;
    final Class<?> [] parameters;

    /**
     * Create the method definition data structure.
     * 
     * @param name method name
     * @param parameters method parameter types
     */
    public MethodDef(String name, Class<?>... parameters) {
        this.name = name;
        this.parameters = parameters;
    }
}
