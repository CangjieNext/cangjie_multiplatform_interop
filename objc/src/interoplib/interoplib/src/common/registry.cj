// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package interoplib.common

import std.sync.Mutex

public let REGISTRY: Registry = Registry()

// TODO: make lock-free concurrent if it will improve performance
public class Registry {
    private var mockRegistry = Array<Any>(10000) {_ => unsafe { zeroValue<Any>() }}
    private var objId: Int64
    private var freeList: Array<Int64>
    private var freeSize: Int64

    private let mutex = Mutex()

    Registry() {
        objId = 0
        freeList = Array<Int64>(10000) {_ => -1}
        freeSize = 0
    }

    public unsafe func put(any: Any): Int64 {
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
                grow()
            }
            mockRegistry[id] = any
            objId++
            id
        }
    }

    public unsafe func get<T>(id: Int64): ?T {
        mockRegistry[id] as T
    }

    public unsafe func remove(id: Int64): Unit {
        synchronized(mutex) {
            mockRegistry[id] = zeroValue<Any>()

            if (freeSize == freeList.size) {
                growFreeList()
            }
            freeList[freeSize] = id
            freeSize++
        }
    }

    private unsafe func grow() {
        let newSize = mockRegistry.size * 2
        let cloneRegistry = Array<Any>(newSize) {_ => zeroValue<Any>() }
        mockRegistry.copyTo(cloneRegistry)
        mockRegistry = cloneRegistry
    }

    private unsafe func growFreeList() {
        let newSize = freeList.size * 2
        let cloneFreeList = Array<Int64>(newSize) {_ => -1}
        freeList.copyTo(cloneFreeList)
        freeList = cloneFreeList
    }
}
