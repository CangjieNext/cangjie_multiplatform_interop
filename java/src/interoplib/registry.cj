// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package interoplib.interop

import std.sync.*

let SIZE_LIMIT = 48 * 1024

var registry = Registry()
private var mutex = Mutex()

public class Registry {
    var mockRegistry: Array<Any>
    var objId: Int64
    var freeList: Array<Int64>
    var freeSize: Int64

    init() {
        mockRegistry = Array<Any>(SIZE_LIMIT) { _ => unsafe { zeroValue<Any>() } }
        objId = 0
        freeList = Array<Int64>(SIZE_LIMIT) { _ => -1 }
        freeSize = 0
    }

    public func put(any: Any): Int64 {
        synchronized(mutex) {
            var id: Int64

            if (freeSize > 0) {
                freeSize--
                id = freeList[freeSize]
                mockRegistry[id] = any
                freeList[freeSize] = -1
                return id
            }

            id = objId
            if (id == mockRegistry.size) {
                throw Exception("Live object limit exceeded. Limit is ${SIZE_LIMIT}")
            }
            mockRegistry[id] = any
            objId++
            id
        }
    }

    public func get<T>(id: Int64): ?T {
        synchronized(mutex) {
            mockRegistry[id] as T
        }
    }

    public func remove(id: Int64): Unit {
        synchronized(mutex) {
            mockRegistry[id] = unsafe { zeroValue<Any>() }

            freeList[freeSize] = id
            freeSize++
        }
    }
}