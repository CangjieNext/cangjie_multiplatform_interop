// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package objc.internal

import std.sync.AtomicBool
import std.sync.Lock

/**
 * Test and test-and-set spin lock implementation.
 */
struct SpinLock <: Lock {
    private let flag = AtomicBool(false)

    public func lock(): Unit {
        // flag.load() spins locally on the cache line while the lock is held.
        // Only when it reads false does the flag.swap(true) execute.
        // If the swap returns true (someone else grabbed it first), the loop continues back to the read-spin.
        // If it returns false, both conditions are false and the loop exits and the lock is acquired.
        while (flag.load() || flag.swap(true)) {}
    }

    public func tryLock(): Bool {
        // Atomically set the flag and check the previous value.
        // If it was false (unlocked), we have acquired the lock; if true, the lock was already held.
        !flag.swap(true)
    }

    public func unlock(): Unit {
        flag.store(false)
    }
}