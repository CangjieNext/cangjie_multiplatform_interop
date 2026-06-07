// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#pragma once
#ifndef SYMBOL_H
#define SYMBOL_H

#include <array>
#include <cassert>
#include <unordered_set>

#include "Collection.h"
#include "InputFile.h"

namespace objcgen {

class NonTypeSymbol;
class Package;
class PackageFile;
class SymbolVisitor;
class TypeDeclarationSymbol;
class TypeMapping;

enum class PrintFormat {
    // The object is printed "as is".  This is good for readability (for example, in
    // diagnostic texts or comments), but syntactic Cangjie correctness is not
    // guaranteed.
    Raw,

    // The object is being emitted as a part of Cangjie code.  The following
    // formatting is applied:
    //     - Generic type names are printed with their type arguments erased.  That
    //       is,
    //           NSArray<ElementT>
    //       is printed as
    //           NSArray/*<ElementT>*/
    //     - Generic type arguments are erased to `ObjCId`.  That is,
    //           - (ElementT) firstObject;
    //       is printed as
    //           - (ObjCId /*ElementT*/) firstObject;
    EmitCangjie,

    // Same as EmitCangjie plus the following additional formatting is applied:
    //     - CPointer is printed as ObjCPointer
    //     - CFunc is printed as ObjCFunc
    EmitCangjieStrict,
};

class NonCopyable {
protected:
    NonCopyable() = default;

    ~NonCopyable() = default;

private:
    NonCopyable(const NonCopyable&) = delete;

    NonCopyable& operator=(const NonCopyable&) = delete;
};

class Symbol {
public:
    [[nodiscard]] const std::string& name() const noexcept
    {
        return name_;
    }

    virtual void rename(std::string_view new_name);

    virtual void print(std::ostream& stream, PrintFormat format) const;

protected:
    [[nodiscard]] explicit Symbol(std::string name) noexcept;

    virtual ~Symbol() = default;

private:
    std::string name_;
};

enum class OutputStatus { Undefined, Root, Referenced, ReferencedMarked, MultiReferenced };

class FileLevelSymbol : public Symbol {
public:
    [[nodiscard]] virtual bool is_file_level() const noexcept = 0;

    [[nodiscard]] virtual bool is_ctype() const noexcept
    {
        return false;
    }

    void set_definition_location(const Location& location);

    [[nodiscard]] const std::unordered_set<FileLevelSymbol*>& references_symbols() const noexcept
    {
        return references_symbols_;
    }

    [[nodiscard]] bool add_reference(FileLevelSymbol& symbol);

    [[nodiscard]] InputFile* defining_file() const noexcept
    {
        return input_file_;
    }

    [[nodiscard]] PackageFile* package_file() const noexcept
    {
        return output_file_;
    }

    void set_package_file(PackageFile& package_file) noexcept;

    void add_referencing_package(const Package& package);

    [[nodiscard]] const std::string& cangjie_package_name() const noexcept;

    [[nodiscard]] Package* package() const noexcept;

    [[nodiscard]] OutputStatus output_status() const noexcept
    {
        return output_status_;
    }

    void set_output_status(OutputStatus output_status) noexcept
    {
        output_status_ = output_status;
    }

    [[nodiscard]] size_t number_of_referencing_packages() const noexcept;

    void print_referencing_packages_info() const;

    template <class To> [[nodiscard]] bool is() const noexcept
    {
        return dynamic_cast<const To*>(this) != nullptr;
    }

    template <class To> [[nodiscard]] const To& as() const noexcept
    {
        assert(is<To>());
        return static_cast<const To&>(*this);
    }

    template <class To> [[nodiscard]] To& as() noexcept
    {
        assert(is<To>());
        return static_cast<To&>(*this);
    }

protected:
    explicit FileLevelSymbol(std::string name) noexcept : Symbol(std::move(name))
    {
    }

private:
    friend class SymbolVisitor;

    virtual void visit_impl(SymbolVisitor& visitor) const = 0;

    // Applicable only for symbols with the same defining file
    friend bool operator<(const FileLevelSymbol& symbol1, const FileLevelSymbol& symbol2) noexcept;

    void set_cangjie_package_name(std::string cangjie_package_name) noexcept;

    InputFile* input_file_ = nullptr; // Stage 1
    LineCol location_{};
    std::unordered_set<FileLevelSymbol*> references_symbols_; // Stage 2

    std::string cangjie_package_name_;
    PackageFile* output_file_ = nullptr; // Stage 3

    OutputStatus output_status_ = OutputStatus::Undefined;

    size_t number_of_referencing_packages_ = 0;

    // Used only for debug print when verbosity > LogLevel::WARNING
    std::unordered_set<const Package*> referencing_packages_;
};

class TypeLikeSymbol : public FileLevelSymbol {
public:
    [[nodiscard]] virtual bool is_unit() const noexcept
    {
        return false;
    }

    [[nodiscard]] virtual bool contains_pointer_or_func() const noexcept
    {
        return false;
    }

    /** Return true if this is a reference type that can be wrapped in Option */
    [[nodiscard]] virtual bool is_optionable_reference() const noexcept
    {
        return false;
    }

    [[nodiscard]] virtual TypeLikeSymbol& map() = 0;

protected:
    explicit TypeLikeSymbol(std::string name) noexcept : FileLevelSymbol(std::move(name))
    {
    }

private:
    /**
     * Return the canonical type for `this`, in the sense of the
     * `clang_getCanonicalType` API.  That is, either this type symbol itself, or
     * its underlying type in case of typedef.
     */
    [[nodiscard]] virtual const TypeLikeSymbol& canonical_type_symbol() const noexcept
    {
        return *this;
    }
};

enum class Nullability { Unspecified, Nullable, Nonnull };

class Type {
public:
    enum class Kind { Unit, Named, TypeParam, VArray, Pointer, Function, Block, Unexposed };

    Type() = default;

    Type(TypeLikeSymbol& symbol, std::vector<Type>&& parameters,
        Nullability nullability = Nullability::Unspecified) noexcept;

    explicit Type(TypeLikeSymbol& symbol, Nullability nullability = Nullability::Unspecified) noexcept;

    Type(Type varray_element_type, size_t varray_size);

    [[nodiscard]] bool has_symbol_assigned() const noexcept
    {
        return symbol_;
    }

    [[nodiscard]] Kind kind() const noexcept
    {
        return kind_;
    }

    [[nodiscard]] bool is_unit() const noexcept;

    [[nodiscard]] const TypeLikeSymbol& symbol() const noexcept;

    [[nodiscard]] TypeLikeSymbol& symbol() noexcept;

    [[nodiscard]] const std::string& name() const noexcept;

    [[nodiscard]] const std::vector<Type>& parameters() const noexcept
    {
        return parameters_;
    }

    void set_parameters(std::vector<Type>&& parameters) noexcept
    {
        parameters_ = std::move(parameters);
    }

    [[nodiscard]] const TypeDeclarationSymbol& actual_protocol() const noexcept;

    [[nodiscard]] Nullability nullability() const noexcept
    {
        return nullability_;
    }

    void set_nullability(Nullability nullability) noexcept;

    [[nodiscard]] const Type& varray_element_type() const noexcept;

    [[nodiscard]] size_t varray_size() const noexcept
    {
        return varray_size_;
    }

    void visit_impl(SymbolVisitor& visitor) const;

    [[nodiscard]] bool is_ctype() const noexcept;

    [[nodiscard]] bool contains_pointer_or_func() const noexcept;

    /**
     * Return the canonical type for `this`, in the sense of the
     * `clang_getCanonicalType` API.  That is, either the type itself, or its
     * underlying type in case of typedef.
     *
     * Actual nullability is taken into account.  That is, having this Objective-C
     * definitions:
     *   typedef T* Alias; // nonnull by default
     *   typedef Alias _Nullable Alias2;
     * the canonical type for Alias will be T, and the canonical type for Alias2
     * will be Option<T>.
     */
    [[nodiscard]] Type canonical_type() const;

    /**
     * Return the canonical type for `this`, in the sense of the
     * `clang_getCanonicalType` API.  That is, either this type symbol itself, or
     * its underlying type in case of typedef.
     */
    [[nodiscard]] const TypeLikeSymbol& canonical_type_symbol() const noexcept;

    /** Return true if this is a reference type that can be wrapped in Option */
    [[nodiscard]] bool is_optionable_reference() const noexcept;

    /**
     * Return true if this is Option (or alias to Option) that needs prepending
     * with '?' when printing.
     */
    [[nodiscard]] bool is_cj_direct_option() const noexcept;

    /**
     * Return true if this is actually an Option, directly or indirectly.  That is,
     * this type accepts None as a value.
     */
    [[nodiscard]] bool is_cj_option() const noexcept;

    void map();

    void print(std::ostream& stream, PrintFormat format) const;

    void print_default_value(std::ostream& stream, PrintFormat format) const;

private:
    [[nodiscard]] Nullability init_nullability(Nullability nullability) noexcept;

    void print_func_like(std::ostream& stream, std::string_view name, PrintFormat format) const;

    Kind kind_ = Kind::Unit;
    TypeLikeSymbol* symbol_ = nullptr;
    std::vector<Type> parameters_;
    union {
        Nullability nullability_ = Nullability::Unspecified;
        size_t varray_size_; // used with VArraySymbol
    };
};

class NamedTypeSymbol : public TypeLikeSymbol {
public:
    enum class Kind : std::uint8_t {
        Unexposed,
        Primitive,
        BuiltIn,
        TypeDef,
        Protocol,
        Interface,
        Struct,
        Union,
        Enum,
        Category,
        TopLevel,
    };

    void rename(std::string_view new_name) override;

    void print(std::ostream& stream, PrintFormat) const override;

    [[nodiscard]] Kind kind() const noexcept
    {
        return kind_;
    }

    [[nodiscard]] bool is(const Kind kind) const noexcept
    {
        return kind_ == kind;
    }

    void set_mapping(const TypeMapping& mapping) noexcept;

protected:
    explicit NamedTypeSymbol(const Kind kind, std::string name) noexcept : TypeLikeSymbol(std::move(name)), kind_(kind)
    {
    }

private:
    [[nodiscard]] TypeLikeSymbol& map() override;

    [[nodiscard]] bool is_optionable_reference() const noexcept override;

    [[nodiscard]] const TypeMapping* mapping() const noexcept
    {
        return mapping_;
    }

    const TypeMapping* mapping_ = nullptr;
    const Kind kind_;
};

/**
 * Just 4 objects of this type exist: Universe::pointer(), Universe::func(),
 * Universe::block(), Universe::varray().  They correspond respectively to
 * ObjCPointer/CPointer, ObjCFunc/CFunc, ObjCBlock, VArray.
 */
class BuiltInTypeSymbol final : public NamedTypeSymbol {
public:
    explicit BuiltInTypeSymbol(std::string name) : NamedTypeSymbol(Kind::BuiltIn, std::move(name))
    {
    }

private:
    [[nodiscard]] bool is_file_level() const noexcept override
    {
        return true;
    }

    void visit_impl(SymbolVisitor&) const override
    {
    }

    [[nodiscard]] TypeLikeSymbol& map() override
    {
        return *this;
    }
};

class EnumConstantSymbol final : public FileLevelSymbol {
public:
    explicit EnumConstantSymbol(std::string name, const std::array<uint64_t, 2>& value) noexcept
        : FileLevelSymbol(std::move(name)), value_{value[0], value[1]}
    {
    }

    template <class T> [[nodiscard]] std::enable_if_t<sizeof(T) <= 64, T> value() const noexcept
    {
        return static_cast<T>(value_[0]);
    }

    template <class T> [[nodiscard]] std::enable_if_t<sizeof(T) <= 64, T> value128_lo() const noexcept
    {
        return value<T>();
    }

    template <class T> [[nodiscard]] std::enable_if_t<sizeof(T) <= 64, T> value128_hi() const noexcept
    {
        return static_cast<T>(value_[1]);
    }

private:
    [[nodiscard]] bool is_file_level() const noexcept override
    {
        return false;
    }

    [[nodiscard]] bool is_ctype() const noexcept override
    {
        return true;
    }

    void visit_impl(SymbolVisitor&) const override
    {
    }

    // In clang, the enum underlying type can also be a 128bit integral
    uint64_t value_[2];
};

class EnumDeclarationSymbol final : public NamedTypeSymbol {
public:
    explicit EnumDeclarationSymbol(std::string name, NamedTypeSymbol& underlying_type) noexcept
        : NamedTypeSymbol(NamedTypeSymbol::Kind::Enum, std::move(name)), underlying_type_(&underlying_type)
    {
    }

    [[nodiscard]] NamedTypeSymbol& underlying_type() const noexcept;

    [[nodiscard]] EnumConstantSymbol& add_constant(std::string name, const std::array<uint64_t, 2>& value);

    template <class Proc>
    void for_each_constant(Proc proc) const noexcept(noexcept(proc(std::declval<EnumConstantSymbol>())))
    {
        for (const auto& constant : constants_) {
            proc(constant);
        }
    }

private:
    [[nodiscard]] bool is_file_level() const noexcept override
    {
        return true;
    }

    [[nodiscard]] bool is_ctype() const noexcept override
    {
        return true;
    }

    void visit_impl(SymbolVisitor& visitor) const override;

    [[nodiscard]] bool empty() const noexcept
    {
        return constants_.empty();
    }

    NamedTypeSymbol* const underlying_type_;
    std::vector<EnumConstantSymbol> constants_;
};

enum class PrimitiveTypeCategory : std::uint8_t {
    Unit,
    SignedInteger,
    UnsignedInteger,
    FloatingPoint,
    Boolean,
};

enum class PrimitiveSize : uint8_t { Zero = 0, One = 1, Two = 2, Four = 4, Eight = 8 };

class PrimitiveTypeSymbol final : public NamedTypeSymbol {
public:
    [[nodiscard]] PrimitiveTypeSymbol(std::string name, PrimitiveTypeCategory category, PrimitiveSize size) noexcept
        : NamedTypeSymbol(NamedTypeSymbol::Kind::Primitive, std::move(name)), category_(category), size_(size)
    {
    }

    [[nodiscard]] PrimitiveTypeCategory category() const noexcept
    {
        return category_;
    }

    [[nodiscard]] PrimitiveSize size() const noexcept
    {
        return size_;
    }

private:
    [[nodiscard]] bool is_file_level() const noexcept override
    {
        return false;
    }

    [[nodiscard]] bool is_ctype() const noexcept override
    {
        return true;
    }

    void visit_impl(SymbolVisitor&) const override
    {
    }

    [[nodiscard]] bool is_unit() const noexcept override
    {
        return category_ == PrimitiveTypeCategory::Unit;
    }

    const PrimitiveTypeCategory category_;
    const PrimitiveSize size_;
};

/**
 * Either a type that libclang does not provide enough info about
 * (CXType_Unexposed), or an Objective-C type not supported in Cangjie (for
 * example, __int128).
 *
 * For fields and variables, this type is mapped to a primitive or to VArray of
 * the corresponding size.
 *
 * For method parameters and return values, the entire method is commented out
 * in the normal mode, as we cannot guarantee correct parameter passing in that
 * case.
 */
class UnexposedTypeSymbol final : public NamedTypeSymbol {
public:
    [[nodiscard]] UnexposedTypeSymbol(std::string name, size_t size);

    [[nodiscard]] const Type& underlying_type() const noexcept
    {
        return underlying_type_;
    }

private:
    void print(std::ostream& stream, PrintFormat format) const override;

    [[nodiscard]] bool is_file_level() const noexcept override
    {
        return false;
    }

    void visit_impl(SymbolVisitor&) const override
    {
    }

    [[nodiscard]] bool is_ctype() const noexcept override
    {
        return true;
    }

    const Type underlying_type_;
};

using Modifiers = uint16_t;

// Zero means ModifierPublic
constexpr Modifiers ModifierPrivate = 1 << 0;
constexpr Modifiers ModifierProtected = 1 << 1;
constexpr Modifiers ModifierPackage = 1 << 2;
constexpr Modifiers ModifierAccessMask = ModifierPrivate | ModifierProtected | ModifierPackage;

constexpr Modifiers ModifierStatic = 1 << 3;
constexpr Modifiers ModifierReadonly = 1 << 4;
constexpr Modifiers ModifierOverride = 1 << 5;
constexpr Modifiers ModifierOptional = 1 << 6;
constexpr Modifiers ModifierInternalLinkage = 1 << 7; // used for Kind::GlobalFunction
constexpr Modifiers ModifierBitField = 1 << 8;

/**
 * A type parameter, when using inside a generic body, can be constrainted by
 * specific protocols.  Like here in the parameter `x`:
 *
 * @interface A<T> : NSObject
 * - (void) foo: (T <NSCopying>) x;
 * @end
 */
class TypeParameterSymbol final : public TypeLikeSymbol {
public:
    explicit TypeParameterSymbol(std::string type_parameter) noexcept : TypeLikeSymbol(std::move(type_parameter))
    {
    }

private:
    [[nodiscard]] bool is_file_level() const noexcept override
    {
        return false;
    }

    void visit_impl(SymbolVisitor&) const override
    {
    }

    [[nodiscard]] TypeParameterSymbol& map() override
    {
        return *this;
    }

    [[nodiscard]] bool is_optionable_reference() const noexcept override
    {
        return true;
    }
};

class TypeDeclarationSymbol : public NamedTypeSymbol {
public:
    [[nodiscard]] TypeDeclarationSymbol(Kind kind, std::string name) noexcept;

    [[nodiscard]] bool is_ctype() const noexcept override
    {
        return is_ctype_;
    }

    [[nodiscard]] auto bases() const noexcept
    {
        return ConstPointerCollection(bases_);
    }

    [[nodiscard]] auto bases() noexcept
    {
        return PointerCollection(bases_);
    }

    void add_base(TypeDeclarationSymbol& base);

    [[nodiscard]] auto parameters() const noexcept
    {
        return ConstCollection(parameters_);
    }

    [[nodiscard]] auto parameters() noexcept
    {
        return Collection(parameters_);
    }

    [[nodiscard]] size_t parameter_count() const noexcept
    {
        return parameters_.size();
    }

    [[nodiscard]] const TypeParameterSymbol& parameter(size_t index) const noexcept;

    [[nodiscard]] TypeParameterSymbol& parameter(size_t index) noexcept;

    void add_parameter(std::string name);

    [[nodiscard]] auto members() const noexcept
    {
        return ConstCollection(members_);
    }

    [[nodiscard]] auto members() noexcept
    {
        return Collection(members_);
    }

    [[nodiscard]] size_t member_count() const noexcept
    {
        return members_.size();
    }

    [[nodiscard]] const NonTypeSymbol& member(size_t index) const noexcept
    {
        return members_[index];
    }

    [[nodiscard]] NonTypeSymbol& member(size_t index) noexcept
    {
        return members_[index];
    }

    void member_remove(size_t index);

    [[nodiscard]] NonTypeSymbol& add_member_method(std::string name, Type return_type, Modifiers modifiers);

    [[nodiscard]] NonTypeSymbol& add_constructor(std::string name, Type return_type);

    [[nodiscard]] NonTypeSymbol& add_field(std::string name, Type type, Modifiers modifiers);

    [[nodiscard]] NonTypeSymbol& add_instance_variable(std::string name, Type type, Modifiers modifiers = 0);

    [[nodiscard]] NonTypeSymbol& add_property(
        std::string name, std::string getter, std::string setter, Modifiers modifiers);

    [[nodiscard]] bool are_override_return_clashes_resolved() const noexcept
    {
        return override_returns_resolved_;
    }

    void mark_override_return_clashes_resolved() noexcept;

    [[nodiscard]] bool are_static_instance_clashes_resolved() const noexcept
    {
        return static_instance_clashes_resolved_;
    }

    void mark_static_instance_clashes_resolved() noexcept;

private:
    [[nodiscard]] bool is_file_level() const noexcept override
    {
        return true;
    }

    void visit_impl(SymbolVisitor& visitor) const override;

    [[nodiscard]] bool contains_pointer_or_func() const noexcept override
    {
        return contains_pointer_or_func_;
    }

    template <class Pred>
    [[nodiscard]] bool all_of_members(Pred cond) const noexcept(noexcept(cond(std::declval<NonTypeSymbol>())));

    template <class Pred>
    [[nodiscard]] bool any_of_members(Pred cond) const noexcept(noexcept(cond(std::declval<NonTypeSymbol>())));

    std::vector<TypeParameterSymbol> parameters_;
    std::vector<NonTypeSymbol> members_;
    std::vector<TypeDeclarationSymbol*> bases_;
    bool is_ctype_ : 1;
    bool contains_pointer_or_func_ : 1;
    bool static_instance_clashes_resolved_ : 1;
    bool override_returns_resolved_ : 1;
};

class TypeAliasSymbol final : public NamedTypeSymbol {
public:
    explicit TypeAliasSymbol(std::string name) noexcept;

    void print(std::ostream& stream, PrintFormat format) const override;

    /**
     * Return the canonical type for `this`, in the sense of the
     * `clang_getCanonicalType` API.  That is, either this type symbol itself, or
     * its underlying type in case of typedef.
     */
    [[nodiscard]] const TypeLikeSymbol& canonical_type_symbol() const noexcept override
    {
        return target_.canonical_type_symbol();
    }

    [[nodiscard]] const Type& target() const noexcept
    {
        return target_;
    }

    [[nodiscard]] Type& target() noexcept
    {
        return target_;
    }

    void set_target(Type target) noexcept
    {
        // It could makes sense to check the underlying type is identical
        target_ = std::move(target);
    }

    /**
     * Return the canonical type for `this`, in the sense of the
     * `clang_getCanonicalType` API.  That is, either the type itself, or its
     * underlying type in case of typedef.
     *
     * Actual nullability is taken into account.  That is, having this Objective-C
     * definitions:
     *   typedef T* Alias; // nonnull by default
     *   typedef Alias _Nullable Alias2;
     * the canonical type for Alias will be T, and the canonical type for Alias2
     * will be Option<T>.
     */
    [[nodiscard]] Type canonical_type() const
    {
        return target_.canonical_type();
    }

private:
    [[nodiscard]] bool is_file_level() const noexcept override
    {
        return true;
    }

    [[nodiscard]] bool is_ctype() const noexcept override
    {
        return target_.has_symbol_assigned() && target_.is_ctype();
    }

    void visit_impl(SymbolVisitor& visitor) const override;

    [[nodiscard]] bool contains_pointer_or_func() const noexcept override
    {
        return target_.has_symbol_assigned() && target_.contains_pointer_or_func();
    }

    Type target_;
};

class ParameterSymbol final : public Symbol {
public:
    ParameterSymbol(std::string name, Type type) noexcept : Symbol(std::move(name)), type_(std::move(type))
    {
    }

    [[nodiscard]] const Type& type() const noexcept
    {
        return type_;
    }

    [[nodiscard]] Type& type() noexcept
    {
        return type_;
    }

    void set_type(Type type) noexcept
    {
        type_ = std::move(type);
    }

private:
    Type type_;
};

class NonTypeSymbol final : public FileLevelSymbol {
public:
    enum class Kind : std::uint8_t {
        Field,
        Property,
        InstanceVariable,
        GlobalFunction, // NOTE: must have stable address and live forever
        MemberMethod,
        Constructor
    };

    [[nodiscard]] NonTypeSymbol(std::string name, Kind kind, Type return_type, Modifiers modifiers = 0) noexcept
        : FileLevelSymbol(std::move(name)), kind_(kind), modifiers_(modifiers), return_type_(std::move(return_type))
    {
    }

    [[nodiscard]] NonTypeSymbol(std::string name, std::string getter, std::string setter, Modifiers modifiers) noexcept
        : FileLevelSymbol(std::move(name)),
          kind_(Kind::Property),
          modifiers_(modifiers),
          getter_(std::move(getter)),
          setter_(std::move(setter))
    {
    }

    void rename(std::string_view new_name) override;

    [[nodiscard]] bool is_ctype() const noexcept override;

    void visit_impl(SymbolVisitor& visitor) const override;

    [[nodiscard]] Kind kind() const noexcept
    {
        return kind_;
    }

    [[nodiscard]] const std::string& selector_attribute() const noexcept
    {
        return selector_attribute_;
    }

    [[nodiscard]] bool is_member_method() const noexcept
    {
        return kind_ == Kind::MemberMethod;
    }

    [[nodiscard]] bool is_constructor() const noexcept
    {
        return kind_ == Kind::Constructor;
    }

    [[nodiscard]] bool is_global_function() const noexcept
    {
        return kind_ == Kind::GlobalFunction;
    }

    [[nodiscard]] bool is_method() const noexcept
    {
        return is_member_method() || is_constructor() || is_global_function();
    }

    [[nodiscard]] bool is_field() const noexcept
    {
        return kind_ == Kind::Field;
    }

    [[nodiscard]] bool is_instance_variable() const noexcept
    {
        return kind_ == Kind::InstanceVariable;
    }

    [[nodiscard]] bool is_property() const noexcept
    {
        return kind_ == Kind::Property;
    }

    [[nodiscard]] const std::string& selector() const noexcept
    {
        return selector_attribute_.empty() ? name() : selector_attribute_;
    }

    [[nodiscard]] const Type& return_type() const noexcept;

    [[nodiscard]] Type& return_type() noexcept;

    void set_return_type(Type return_type) noexcept;

    [[nodiscard]] auto parameters() const noexcept
    {
        return ConstCollection(parameters_);
    }

    [[nodiscard]] auto parameters() noexcept
    {
        return Collection(parameters_);
    }

    [[nodiscard]] size_t parameter_count() const noexcept
    {
        return parameters_.size();
    }

    [[nodiscard]] ParameterSymbol& parameter(size_t index) noexcept
    {
        return parameters_[index];
    }

    [[nodiscard]] const ParameterSymbol& parameter(size_t index) const noexcept
    {
        return parameters_[index];
    }

    void add_parameter(std::string name, Type type);

    [[nodiscard]] bool is_public() const noexcept
    {
        return !(modifiers_ & ModifierAccessMask);
    }

    [[nodiscard]] bool is_protected() const noexcept
    {
        return (modifiers_ & ModifierAccessMask) == ModifierProtected;
    }

    [[nodiscard]] bool is_static() const noexcept
    {
        return modifiers_ & ModifierStatic;
    }

    [[nodiscard]] bool is_instance() const noexcept
    {
        return !is_static();
    }

    [[nodiscard]] bool is_readonly() const noexcept
    {
        return modifiers_ & ModifierReadonly;
    }

    [[nodiscard]] bool is_override() const noexcept
    {
        return modifiers_ & ModifierOverride;
    }

    void set_override() noexcept
    {
        modifiers_ |= ModifierOverride;
    }

    [[nodiscard]] bool is_objc_optional() const noexcept
    {
        return modifiers_ & ModifierOptional;
    }

    [[nodiscard]] bool has_internal_linkage() const noexcept
    {
        return modifiers_ & ModifierInternalLinkage;
    }

    [[nodiscard]] const std::string& getter() const noexcept
    {
        return getter_;
    }

    [[nodiscard]] const std::string& setter() const noexcept
    {
        return setter_;
    }

    [[nodiscard]] bool is_bit_field() const noexcept
    {
        return modifiers_ & ModifierBitField;
    }

private:
    [[nodiscard]] bool is_file_level() const noexcept override
    {
        return is_global_function();
    }

    Kind kind_;
    Modifiers modifiers_;

    // used for Kind::Property
    std::string getter_;
    std::string setter_;

    Type return_type_;
    std::vector<ParameterSymbol> parameters_;
    std::string selector_attribute_;
};

template <class T> class Printer {
public:
    Printer(const T& obj, PrintFormat format) noexcept : obj_(obj), format_(format)
    {
    }

    [[nodiscard]] const T& obj() const noexcept
    {
        return obj_;
    }

    [[nodiscard]] PrintFormat format() const noexcept
    {
        return format_;
    }

private:
    friend std::ostream& operator<<(std::ostream& stream, const Printer& printer)
    {
        printer.obj_.print(stream, printer.format_);
        return stream;
    }

    const T& obj_;
    const PrintFormat format_;
};

template <class T> [[nodiscard]] Printer<T> raw(const T& obj) noexcept
{
    return {obj, PrintFormat::Raw};
}

template <class T> [[nodiscard]] Printer<T> emit_cangjie(const T& obj) noexcept
{
    return {obj, PrintFormat::EmitCangjie};
}

template <class T> [[nodiscard]] Printer<T> emit_cangjie_strict(const T& obj) noexcept
{
    return {obj, PrintFormat::EmitCangjieStrict};
}

class KeywordEscaper {
public:
    explicit KeywordEscaper(std::string_view name) noexcept : name(name)
    {
    }

private:
    friend std::ostream& operator<<(std::ostream& stream, const KeywordEscaper& op);

    const std::string_view name;
};

[[nodiscard]] inline KeywordEscaper escape_keyword(std::string_view name) noexcept
{
    return KeywordEscaper(name);
}

} // namespace objcgen

#endif // SYMBOL_H
