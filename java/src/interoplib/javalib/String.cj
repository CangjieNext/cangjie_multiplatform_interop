// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package java.lang

import java.internal.*

@JavaMirror["java.lang.String"]
public class JString <: JObject {
    public init()
    // There is an implicit interop intrinsic: init(value: String)
    public init(arg0: ?JString)
    public init(arg0: ?JArray<Int8>)
    public init(arg0: ?JArray<Int8>, arg1: Int32, arg2: Int32)
    public init(arg0: ?JArray<Int8>, arg1: ?JString)
    public init(arg0: ?JArray<Int8>, arg1: Int32, arg2: Int32, arg3: ?JString)
    public init(arg0: ?JArray<Int8>, arg1: Int32)
    public init(arg0: ?JArray<Int8>, arg1: Int32, arg2: Int32, arg3: Int32)
    public init(arg0: ?JArray<Int32>, arg1: Int32, arg2: Int32)
    public init(arg0: ?JArray<UInt16 /* java char */>, arg1: Int32, arg2: Int32)
    public init(arg0: ?JArray<UInt16 /* java char */>)
    public func toJString(): JString
    public func compareTo(arg0: JString): Int32
    public static func valueOf(arg0: Float64): JString
    public static func valueOf(arg0: Float32): JString
    public static func valueOf(arg0: Int64): JString
    public static func valueOf(arg0: Int32): JString
    public static func valueOf(arg0: UInt16 /* java char */): JString
    public static func valueOf(arg0: Bool): JString
    public static func valueOf(arg0: ?JArray<UInt16>, arg1: Int32, arg2: Int32): JString
    public static func valueOf(arg0: ?JArray<UInt16>): JString
    public static func valueOf(arg0: ?JObject): JString
    public func replaceAll(arg0: ?JString, arg1: ?JString): JString
    public func isEmpty(): Bool
    public func length(): Int32
    public func substring(arg0: Int32, arg1: Int32): JString
    public func substring(arg0: Int32): JString
    public func startsWith(arg0: ?JString): Bool
    public func startsWith(arg0: ?JString, arg1: Int32): Bool
    public func repeat(arg0: Int32): ?JString
    public static func copyValueOf(arg0: ?JArray<UInt16>): JString
    public static func copyValueOf(arg0: ?JArray<UInt16>, arg1: Int32, arg2: Int32): JString
    public func formatted(arg0: ?JArray<?JObject>): JString
    public static func format(arg0: ?JString, arg1: ?JArray<?JObject>): JString
    public func toCharArray(): JArray<UInt16>
    public func translateEscapes(): JString
    public func stripIndent(): ?JString
    public func indent(arg0: Int32): ?JString
    public func isBlank(): Bool
    public func stripTrailing(): ?JString
    public func stripLeading(): ?JString
    public func strip(): ?JString
    public func trim(): ?JString
    public func toUpperCase(): ?JString
    public func toLowerCase(): ?JString
    public func split(arg0: ?JString): ?JArray<?JString>
    public func split(arg0: ?JString, arg1: Int32): ?JArray<?JString>
    public func replaceFirst(arg0: ?JString, arg1: ?JString): ?JString
    public func matches(arg0: ?JString): Bool
    public func replace(arg0: UInt16, arg1: UInt16): ?JString
    public func concat(arg0: JString): ?JString
    public func lastIndexOf(arg0: ?JString, arg1: Int32): Int32
    public func lastIndexOf(arg0: ?JString): Int32
    public func indexOf(arg0: ?JString, arg1: Int32): Int32
    public func indexOf(arg0: ?JString): Int32
    public func lastIndexOf(arg0: Int32, arg1: Int32): Int32
    public func lastIndexOf(arg0: Int32): Int32
    public func indexOf(arg0: Int32, arg1: Int32): Int32
    public func indexOf(arg0: Int32): Int32
    public func hashCode32(): Int32
    public func endsWith(arg0: ?JString): Bool
    public func regionMatches(arg0: Bool, arg1: Int32, arg2: ?JString, arg3: Int32, arg4: Int32): Bool
    public func regionMatches(arg0: Int32, arg1: JString, arg2: Int32, arg3: Int32): Bool
    public func compareToIgnoreCase(arg0: ?JString): Int32
    public func equalsIgnoreCase(arg0: ?JString): Bool
    public func equals(arg0: ?JObject): Bool
    public func getBytes(): ?JArray<Int8>
    public func getBytes(arg0: ?JString): ?JArray<Int8>
    public func getBytes(arg0: Int32, arg1: Int32, arg2: ?JArray<Int8>, arg3: Int32): Unit
    public func getChars(arg0: Int32, arg1: Int32, arg2: ?JArray<UInt16>, arg3: Int32): Unit
    public func offsetByCodePoints(arg0: Int32, arg1: Int32): Int32
    public func codePointCount(arg0: Int32, arg1: Int32): Int32
    public func codePointBefore(arg0: Int32): Int32
    public func codePointAt(arg0: Int32): Int32
    public func charAt(arg0: Int32): UInt16
}
