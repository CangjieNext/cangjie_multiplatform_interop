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
#include <deque>
#include <unordered_set>

#include "InputFile.h"

namespace objcgen {

class NonTypeSymbol;
class Package;
class PackageFile;
class ParameterSymbol;
class Symbol;
struct TypeMapping;

enum class SymbolPrintFormat {
    // The symbol is printed "as is".  This is good for readability (for example, in
    // diagnostic texts or comments), but syntactic Cangjie correctness is not
    // guaranteed.
    Raw,

    // The symbol is being emitted as a part of Cangjie code.  The following
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

class SymbolPrinter {
public:
    SymbolPrinter(const Symbol& symbol, SymbolPrintFormat format) noexcept : symbol_(symbol), format_(format)
    {
    }

    friend std::ostream& operator<<(std::ostream& stream, const SymbolPrinter& symbolPrinter);

    const auto& symbol() const noexcept
    {
        return symbol_;
    }

    const auto& format() const noexcept
    {
        return format_;
    }

private:
    const Symbol& symbol_;
    const SymbolPrintFormat format_;
};

inline SymbolPrinter raw(const Symbol& symbol) noexcept
{
    return {symbol, SymbolPrintFormat::Raw};
}

inline SymbolPrinter emit_cangjie(const Symbol& symbol) noexcept
{
    return {symbol, SymbolPrintFormat::EmitCangjie};
}

inline SymbolPrinter emit_cangjie_strict(const Symbol& symbol) noexcept
{
    return {symbol, SymbolPrintFormat::EmitCangjieStrict};
}

class KeywordEscaper {
public:
    KeywordEscaper(std::string_view name) noexcept : name(name)
    {
    }

    friend std::ostream& operator<<(std::ostream& stream, const KeywordEscaper& op);

private:
    const std::string_view name;
};

inline KeywordEscaper escape_keyword(std::string_view name)
{
    return KeywordEscaper(name);
}

template <typename Symbol, typename T> struct SymbolChildren {
    Symbol* self_ = nullptr;

    [[nodiscard]] explicit SymbolChildren(Symbol* self) : self_(self)
    {
        assert(self);
    }

    class ItemIterator final {
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = T;
        using pointer = value_type*;   // or also value_type*
        using reference = value_type&; // or also value_type&

        SymbolChildren& container_;
        std::size_t index_;

    public:
        ItemIterator(SymbolChildren& container, std::size_t index) : container_(container), index_(index)
        {
        }

        reference operator*() const
        {
            return container_.get(index_);
        }

        pointer operator->()
        {
            return std::addressof(operator*());
        }

        // Prefix increment
        ItemIterator& operator++()
        {
            ++index_;
            return *this;
        }

        // Postfix increment
        ItemIterator operator++(int)
        {
            auto tmp = *this;
            ++(*this);
            return tmp;
        }

        friend bool operator==(const ItemIterator& lhs, const ItemIterator& rhs)
        {
            return lhs.index_ == rhs.index_;
        }

        friend bool operator!=(const ItemIterator& lhs, const ItemIterator& rhs)
        {
            return !(lhs == rhs);
        }
    };

    [[nodiscard]] virtual T& get(std::size_t index) const = 0;

    [[nodiscard]] virtual std::size_t size() const = 0;

    virtual ~SymbolChildren() = default;

    ItemIterator begin()
    {
        return {*this, 0};
    }

    ItemIterator end()
    {
        return {*this, size()};
    }
};

class Symbol {
    std::string name_;

public:
    [[nodiscard]] const std::string& name() const noexcept
    {
        return name_;
    }

    virtual void rename(const std::string_view new_name)
    {
        assert(!new_name.empty());
        name_ = new_name;
    }

    virtual void print(std::ostream& stream, SymbolPrintFormat format) const;

protected:
    [[nodiscard]] explicit Symbol(std::string name) noexcept;

    virtual ~Symbol() = default;
};

std::ostream& operator<<(std::ostream& stream, const SymbolPrinter& symbolPrinter);

enum class SymbolProperty {
    None,
    TypeArgument,
    Base,
    Member,
    TupleItem,
    FunctionParametersTuple,
    FunctionReturnType,
    AliasTarget,
    ParameterType,
    ReturnType,
};

class SymbolVisitor {
public:
    virtual ~SymbolVisitor() = default;

    void recurse(FileLevelSymbol* value);

    void visit(FileLevelSymbol* owner, FileLevelSymbol* value, const SymbolProperty property)
    {
        assert((!!owner) == (property != SymbolProperty::None));
        if (value) {
            visit_impl(owner, value, property);
        }
    }

    void visit(FileLevelSymbol* symbol)
    {
        visit(nullptr, symbol, SymbolProperty::None);
    }

protected:
    virtual void visit_impl(FileLevelSymbol* owner, FileLevelSymbol* value, SymbolProperty property) = 0;
};

enum class OutputStatus { Undefined, Root, Referenced, ReferencedMarked, MultiReferenced };

class FileLevelSymbol : public Symbol {
    InputFile* input_file_ = nullptr; // Stage 1
    LineCol location_ = {0, 0};
    std::unordered_set<FileLevelSymbol*> references_symbols_; // Stage 2

    std::string cangjie_package_name_;
    PackageFile* output_file_ = nullptr; // Stage 3

    OutputStatus output_status_ = OutputStatus::Undefined;

    size_t number_of_referencing_packages_ = 0;

    // Used only for debug print when verbosity > LogLevel::WARNING
    std::unordered_set<const Package*> referencing_packages_;

public:
    // Applicable only for symbols with the same defining file
    friend bool operator<(const FileLevelSymbol& symbol1, const FileLevelSymbol& symbol2) noexcept
    {
        assert(symbol1.input_file_);
        assert(symbol1.input_file_ == symbol2.input_file_);
        return symbol1.location_ < symbol2.location_;
    }

    [[nodiscard]] const std::string& cangjie_package_name() const;

    [[nodiscard]] Package* package() const;

    [[nodiscard]] PackageFile* package_file() const
    {
        return output_file_;
    }

    [[nodiscard]] virtual bool is_file_level() const noexcept = 0;

    [[nodiscard]] virtual InputFile* defining_file() const noexcept
    {
        return input_file_;
    }

    [[nodiscard]] virtual bool is_ctype() const noexcept
    {
        return false;
    }

    void set_definition_location(const Location& location);

    void set_package_file(PackageFile* package_file);

    void set_cangjie_package_name(std::string cangjie_package_name) noexcept;

    [[nodiscard]] const std::unordered_set<FileLevelSymbol*>& references_symbols() const
    {
        return references_symbols_;
    }

    bool add_reference(FileLevelSymbol& symbol);

    [[nodiscard]] auto output_status() const noexcept
    {
        return output_status_;
    }

    void set_output_status(OutputStatus output_status) noexcept
    {
        output_status_ = output_status;
    }

    void add_referencing_package(const Package& package);

    size_t number_of_referencing_packages() const noexcept;

    void print_referencing_packages_info() const;

protected:
    explicit FileLevelSymbol(std::string name) noexcept : Symbol(std::move(name))
    {
    }

    virtual void visit_impl(SymbolVisitor& visitor) = 0;

    friend void SymbolVisitor::recurse(FileLevelSymbol*);
};

class TypeLikeSymbol : public FileLevelSymbol {
public:
    [[nodiscard]] virtual TypeLikeSymbol* map() = 0;

    [[nodiscard]] virtual bool is_unit() const noexcept
    {
        return false;
    }

    [[nodiscard]] bool is_instancetype() const
    {
        return name() == "instancetype";
    }

    /**
     * Return the canonical type for `this`, in the sense of the
     * `clang_getCanonicalType` API.  That is, either the type itself, or its
     * underlying type in case of typedef.
     */
    [[nodiscard]] virtual const TypeLikeSymbol& canonical_type() const
    {
        return *this;
    }

    [[nodiscard]] virtual bool contains_pointer_or_func() const noexcept
    {
        return false;
    }

protected:
    explicit TypeLikeSymbol(std::string name) : FileLevelSymbol(std::move(name))
    {
    }
};

class TypeParameterSymbol final : public TypeLikeSymbol {
    struct Private {
        explicit Private() = default;
    };

    // The only place allowed to create an instance of TypeSymbol.
    friend class TypeDeclarationSymbol;

public:
    explicit TypeParameterSymbol(Private, std::string name);

    [[nodiscard]] TypeLikeSymbol* map() override
    {
        return this;
    }

    [[nodiscard]] bool is_file_level() const noexcept override
    {
        return false;
    }

    void print(std::ostream& stream, SymbolPrintFormat format) const override;

protected:
    void visit_impl(SymbolVisitor&) override
    {
    }
};

/**
 * A type parameter, when using inside a generic body, can be specified with a
 * narrowing protocol.  Like here in the parameter `x`:
 *
 * @interface A<T> : NSObject
 * - (void) foo: (T <NSCopying>) x;
 * @end
 */
class NarrowedTypeParameterSymbol final : public TypeLikeSymbol {
public:
    explicit NarrowedTypeParameterSymbol(const TypeParameterSymbol& type_parameter, std::string protocol_name)
        : TypeLikeSymbol(std::string(type_parameter.name())), protocol_name_(std::move(protocol_name))
    {
    }

    void print(std::ostream& stream, SymbolPrintFormat format) const override;

    void visit_impl(SymbolVisitor&) override
    {
    }

    [[nodiscard]] TypeLikeSymbol* map() override
    {
        return this;
    }

    [[nodiscard]] bool is_file_level() const noexcept override
    {
        return false;
    }

private:
    std::string protocol_name_;
};

class PointerTypeSymbol final : public TypeLikeSymbol {
public:
    PointerTypeSymbol(TypeLikeSymbol& pointee) : TypeLikeSymbol(""), pointee_(&pointee)
    {
    }

    const TypeLikeSymbol& pointee() const noexcept
    {
        return *pointee_;
    }

    void print(std::ostream& stream, SymbolPrintFormat format) const override;

    void visit_impl(SymbolVisitor& visitor) override
    {
        visitor.visit(this, pointee_, SymbolProperty::TypeArgument);
    }

    [[nodiscard]] PointerTypeSymbol* map() override;

    [[nodiscard]] bool is_ctype() const noexcept override
    {
        return pointee_->is_ctype();
    }

    [[nodiscard]] bool contains_pointer_or_func() const noexcept override
    {
        return true;
    }

    [[nodiscard]] bool is_file_level() const noexcept override
    {
        return false;
    }

private:
    TypeLikeSymbol* const pointee_;
};

class VArraySymbol final : public TypeLikeSymbol {
public:
    TypeLikeSymbol* const element_type_;
    const size_t size_;

    explicit VArraySymbol(TypeLikeSymbol& element_type, size_t size)
        : TypeLikeSymbol("VArray"), element_type_(&element_type), size_(size)
    {
    }

    [[nodiscard]] const auto& element_type() const noexcept
    {
        return *element_type_;
    }

    [[nodiscard]] auto& element_type() noexcept
    {
        return *element_type_;
    }

    [[nodiscard]] auto size() const noexcept
    {
        return size_;
    }

    void print(std::ostream& stream, SymbolPrintFormat format) const override;

    void visit_impl(SymbolVisitor& visitor) override
    {
        visitor.visit(this, element_type_, SymbolProperty::TypeArgument);
    }

    [[nodiscard]] VArraySymbol* map() override;

    [[nodiscard]] bool is_ctype() const noexcept override
    {
        return element_type_->is_ctype();
    }

    [[nodiscard]] bool is_file_level() const noexcept override
    {
        return false;
    }
};

class NamedTypeSymbol : public TypeLikeSymbol {
public:
    enum class Kind : std::uint8_t {
        Unexposed,
        Primitive,
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

    [[nodiscard]] bool is(const Kind kind) const
    {
        return kind_ == kind;
    }

    [[nodiscard]] Kind kind() const
    {
        return kind_;
    }

    [[nodiscard]] virtual std::size_t parameter_count() const noexcept = 0;

    [[nodiscard]] virtual TypeLikeSymbol* parameter(std::size_t index) const = 0;

    void print(std::ostream& stream, SymbolPrintFormat format) const override;

    [[nodiscard]] TypeMapping* mapping() const
    {
        return mapping_;
    }

    void set_mapping(TypeMapping* mapping)
    {
        assert(mapping_ == nullptr);
        assert(mapping != nullptr);
        mapping_ = mapping;
    }

    [[nodiscard]] virtual NamedTypeSymbol* original() const noexcept = 0;

    [[nodiscard]] TypeLikeSymbol* map() override;

    [[nodiscard]] virtual NamedTypeSymbol* construct(const std::vector<TypeLikeSymbol*>& arguments) const;

protected:
    explicit NamedTypeSymbol(const Kind kind, std::string name) : TypeLikeSymbol(std::move(name)), kind_(kind)
    {
        // Categories can have empty names
        assert(!this->name().empty() || this->kind_ == Kind::Category || this->kind_ == Kind::TopLevel);
    }

private:
    TypeMapping* mapping_ = nullptr;
    const Kind kind_;

    /**
     * Return `true` if either this symbol satisfies the predicate `cond` or (if
     * this is `TypeAliasSymbol`) the function returns `true` for the target.
     */
    template <class UnaryPred>
    bool is_recursively(UnaryPred cond) const noexcept(noexcept(cond(std::declval<NamedTypeSymbol>())));

    bool is_objc_reference() const noexcept;
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

    template <class T> std::enable_if_t<sizeof(T) <= 64, T> value128_lo() const noexcept
    {
        return value<T>();
    }

    template <class T> [[nodiscard]] std::enable_if_t<sizeof(T) <= 64, T> value128_hi() const noexcept
    {
        return static_cast<T>(value_[1]);
    }

private:
    // In clang, the enum underlying type can also be a 128bit integral
    uint64_t value_[2];

    [[nodiscard]] bool is_file_level() const noexcept override
    {
        return false;
    }

    [[nodiscard]] bool is_ctype() const noexcept override
    {
        return true;
    }

    void visit_impl(SymbolVisitor&) override
    {
    }
};

class EnumDeclarationSymbol final : public NamedTypeSymbol {
public:
    EnumDeclarationSymbol(std::string name) noexcept
        : NamedTypeSymbol(NamedTypeSymbol::Kind::Enum, std::move(name)), underlying_type_(nullptr)
    {
    }

    auto empty() const noexcept
    {
        return constants_.empty();
    }

    NamedTypeSymbol& underlying_type() const noexcept;

    EnumConstantSymbol& add_constant(
        std::string name, NamedTypeSymbol& underlying_type, const std::array<uint64_t, 2>& value);

    template <class Proc>
    void for_each_constant(Proc proc) const noexcept(noexcept(proc(std::declval<EnumConstantSymbol>())))
    {
        for (const auto& constant : constants_) {
            proc(constant);
        }
    }

    [[nodiscard]] size_t parameter_count() const noexcept override
    {
        return 0;
    }

    [[nodiscard]] TypeLikeSymbol* parameter(size_t) const override
    {
        assert(false);
        return nullptr;
    }

private:
    NamedTypeSymbol* underlying_type_;
    std::deque<EnumConstantSymbol> constants_;

    [[nodiscard]] bool is_file_level() const noexcept override
    {
        return true;
    }

    void visit_impl(SymbolVisitor& visitor) override
    {
        if (underlying_type_) {
            visitor.visit(this, underlying_type_, SymbolProperty::ReturnType);
        }
    }

    [[nodiscard]] EnumDeclarationSymbol* original() const noexcept override
    {
        return const_cast<EnumDeclarationSymbol*>(this);
    }

    [[nodiscard]] bool is_ctype() const noexcept override
    {
        return true;
    }
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
    [[nodiscard]] PrimitiveTypeSymbol(std::string name, PrimitiveTypeCategory category, PrimitiveSize size)
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

    [[nodiscard]] bool is_unit() const noexcept override
    {
        return category_ == PrimitiveTypeCategory::Unit;
    }

private:
    PrimitiveTypeCategory category_;
    PrimitiveSize size_;

    [[nodiscard]] bool is_file_level() const noexcept override
    {
        return false;
    }

    void visit_impl(SymbolVisitor&) override
    {
    }

    [[nodiscard]] size_t parameter_count() const noexcept override
    {
        return 0;
    }

    [[nodiscard]] TypeLikeSymbol* parameter(size_t) const override
    {
        assert(false);
        return nullptr;
    }

    [[nodiscard]] PrimitiveTypeSymbol* original() const noexcept override
    {
        return const_cast<PrimitiveTypeSymbol*>(this);
    }

    [[nodiscard]] virtual bool is_ctype() const noexcept override
    {
        return true;
    }
};

/**
 * Either an Objective-C compiler primitive built-in type not properly supported
 * in Cangjie (for example, __int128), or a type that libclang does not provide
 * enough info about (CXType_Unexposed).
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
    [[nodiscard]] UnexposedTypeSymbol(std::string name, const TypeLikeSymbol& type)
        : NamedTypeSymbol(NamedTypeSymbol::Kind::Unexposed, std::move(name)), type_(type)
    {
    }

private:
    [[nodiscard]] bool is_file_level() const noexcept override
    {
        return false;
    }

    void visit_impl(SymbolVisitor&) override
    {
    }

    [[nodiscard]] size_t parameter_count() const noexcept override
    {
        return 0;
    }

    [[nodiscard]] TypeLikeSymbol* parameter(size_t) const override
    {
        assert(false);
        return nullptr;
    }

    [[nodiscard]] UnexposedTypeSymbol* original() const noexcept override
    {
        return const_cast<UnexposedTypeSymbol*>(this);
    }

    [[nodiscard]] bool is_ctype() const noexcept override
    {
        return true;
    }

    [[nodiscard]] const TypeLikeSymbol& canonical_type() const override
    {
        return type_;
    }

    void print(std::ostream& stream, SymbolPrintFormat format) const override;

    const TypeLikeSymbol& type_;
};

// Zero means ModifierPublic
constexpr uint16_t ModifierPrivate = 1 << 0;
constexpr uint16_t ModifierProtected = 1 << 1;
constexpr uint16_t ModifierPackage = 1 << 2;
constexpr uint16_t ModifierAccessMask = ModifierPrivate | ModifierProtected | ModifierPackage;

constexpr uint16_t ModifierStatic = 1 << 3;
constexpr uint16_t ModifierReadonly = 1 << 4;
constexpr uint16_t ModifierNullable = 1 << 5;
constexpr uint16_t ModifierOverride = 1 << 6;
constexpr uint16_t ModifierOptional = 1 << 7;
constexpr uint16_t ModifierInternalLinkage = 1 << 8; // used for Kind::GlobalFunction

class TypeDeclarationSymbol : public NamedTypeSymbol {
    std::vector<std::unique_ptr<TypeParameterSymbol>> parameters_;
    std::vector<NonTypeSymbol> members_;
    std::vector<TypeDeclarationSymbol*> bases_;
    bool is_ctype_;
    bool contains_pointer_or_func_ = false;
    bool static_instance_clashes_resolved_ = false;

public:
    struct ParameterCollection final : SymbolChildren<TypeDeclarationSymbol, TypeParameterSymbol> {
        explicit ParameterCollection(TypeDeclarationSymbol* self) : SymbolChildren(self)
        {
        }

        [[nodiscard]] TypeParameterSymbol& get(const std::size_t index) const override
        {
            auto* parameter = self_->parameter(index);
            assert(parameter);
            return *parameter;
        }

        [[nodiscard]] std::size_t size() const override
        {
            return self_->parameter_count();
        }
    };

    struct MemberCollection final : SymbolChildren<TypeDeclarationSymbol, NonTypeSymbol> {
        explicit MemberCollection(TypeDeclarationSymbol* self) : SymbolChildren(self)
        {
        }

        [[nodiscard]] NonTypeSymbol& get(const std::size_t index) const override
        {
            return self_->member(index);
        }

        [[nodiscard]] std::size_t size() const override
        {
            return self_->member_count();
        }
    };

    struct BaseCollection final : SymbolChildren<TypeDeclarationSymbol, TypeDeclarationSymbol> {
        explicit BaseCollection(TypeDeclarationSymbol* self) : SymbolChildren(self)
        {
        }

        [[nodiscard]] TypeDeclarationSymbol& get(const std::size_t index) const override
        {
            auto* base = self_->base(index);
            assert(base);
            return *base;
        }

        [[nodiscard]] std::size_t size() const override
        {
            return self_->base_count();
        }
    };

    [[nodiscard]] TypeDeclarationSymbol(Kind kind, std::string name);

    [[nodiscard]] bool is_ctype() const noexcept override
    {
        return is_ctype_;
    }

    [[nodiscard]] bool contains_pointer_or_func() const noexcept override
    {
        return contains_pointer_or_func_;
    }

    [[nodiscard]] std::size_t parameter_count() const noexcept override
    {
        return parameters_.size();
    }

    [[nodiscard]] TypeParameterSymbol* parameter(std::size_t index) const override;

    [[nodiscard]] ParameterCollection parameters()
    {
        return ParameterCollection(this);
    }

    [[nodiscard]] std::size_t member_count() const
    {
        return members_.size();
    }

    void member_remove(size_t index);

    [[nodiscard]] const NonTypeSymbol& member(const std::size_t index) const
    {
        return members_.at(index);
    }

    [[nodiscard]] NonTypeSymbol& member(const std::size_t index)
    {
        return members_.at(index);
    }

    [[nodiscard]] MemberCollection members()
    {
        return MemberCollection(this);
    }

    template <class Pred> bool all_of_members(Pred cond) const noexcept(noexcept(cond(std::declval<NonTypeSymbol>())));

    template <class Pred> bool any_of_members(Pred cond) const noexcept(noexcept(cond(std::declval<NonTypeSymbol>())));

    [[nodiscard]] std::size_t base_count() const
    {
        return bases_.size();
    }

    [[nodiscard]] TypeDeclarationSymbol* base(const std::size_t index) const
    {
        return bases_.at(index);
    }

    [[nodiscard]] BaseCollection bases()
    {
        return BaseCollection(this);
    }

    void add_parameter(std::string name);

    NonTypeSymbol& add_member_method(std::string name, TypeLikeSymbol* return_type, uint16_t modifiers);

    NonTypeSymbol& add_constructor(std::string name, TypeLikeSymbol* return_type);

    NonTypeSymbol& add_field(std::string name, TypeLikeSymbol* type, bool is_nullable);

    NonTypeSymbol& add_instance_variable(std::string name, TypeLikeSymbol* type, uint16_t modifiers);

    NonTypeSymbol& add_property(std::string name, std::string getter, std::string setter, uint16_t modifiers);

    void add_base(TypeDeclarationSymbol& base);

    [[nodiscard]] TypeDeclarationSymbol* original() const noexcept override
    {
        return const_cast<TypeDeclarationSymbol*>(this);
    }

    [[nodiscard]] bool is_file_level() const noexcept override
    {
        return true;
    }

    [[nodiscard]] bool are_static_instance_clashes_resolved() const noexcept
    {
        return static_instance_clashes_resolved_;
    }

    void mark_static_instance_clashes_resolved() noexcept
    {
        assert(!static_instance_clashes_resolved_);
        static_instance_clashes_resolved_ = true;
    }

protected:
    void visit_impl(SymbolVisitor& visitor) override;
};

class CategoryDeclarationSymbol final : public TypeDeclarationSymbol {
public:
    explicit CategoryDeclarationSymbol(std::string name, TypeDeclarationSymbol* interface)
        : TypeDeclarationSymbol(Kind::Category, std::move(name)), interface_(interface)
    {
    }

    TypeDeclarationSymbol* interface() const noexcept
    {
        return interface_;
    }

private:
    TypeDeclarationSymbol* const interface_;
};

class TupleTypeSymbol final : public TypeLikeSymbol {
    std::vector<TypeLikeSymbol*> items_;
    bool is_ctype_;
    bool contains_pointer_or_func_;

public:
    explicit TupleTypeSymbol();

    explicit TupleTypeSymbol(std::vector<TypeLikeSymbol*> items);

    [[nodiscard]] bool is_ctype() const noexcept override
    {
        return is_ctype_;
    }

    [[nodiscard]] bool contains_pointer_or_func() const noexcept override
    {
        return contains_pointer_or_func_;
    }

    [[nodiscard]] std::size_t item_count() const noexcept
    {
        return items_.size();
    }

    [[nodiscard]] TypeLikeSymbol* item(std::size_t index) const;

    void add_item(TypeLikeSymbol* parameter)
    {
        assert(parameter);
        if (is_ctype_ && !parameter->is_ctype()) {
            is_ctype_ = false;
        }
        if (parameter->contains_pointer_or_func()) {
            contains_pointer_or_func_ = true;
        }
        items_.push_back(parameter);
    }

    void print(std::ostream& stream, SymbolPrintFormat format) const override;

    [[nodiscard]] TupleTypeSymbol* map() override;

    [[nodiscard]] bool is_file_level() const noexcept override
    {
        return false;
    }

protected:
    void visit_impl(SymbolVisitor& visitor) override;
};

class FuncLikeTypeSymbol : public TypeLikeSymbol {
    TupleTypeSymbol* parameters_ = nullptr;
    TypeLikeSymbol* return_type_ = nullptr;

public:
    [[nodiscard]] TupleTypeSymbol* parameters() const
    {
        return parameters_;
    }

    void set_parameters(TupleTypeSymbol* parameters)
    {
        assert(parameters);
        parameters_ = parameters;
    }

    [[nodiscard]] TypeLikeSymbol* return_type() const
    {
        return return_type_;
    }

    void set_return_type(TypeLikeSymbol* return_type)
    {
        assert(return_type);
        return_type_ = return_type;
    }

    [[nodiscard]] bool is_file_level() const noexcept override
    {
        return false;
    }

protected:
    FuncLikeTypeSymbol();
    FuncLikeTypeSymbol(TupleTypeSymbol* parameters, TypeLikeSymbol* return_type);
    void visit_impl(SymbolVisitor& visitor) override;
    void do_print(std::ostream& stream, std::string_view name, SymbolPrintFormat format) const;
};

class FuncTypeSymbol final : public FuncLikeTypeSymbol {
public:
    FuncTypeSymbol()
    {
    }

    FuncTypeSymbol(TupleTypeSymbol* parameters, TypeLikeSymbol* return_type)
        : FuncLikeTypeSymbol(parameters, return_type)
    {
    }

    [[nodiscard]] bool is_ctype() const noexcept override
    {
        return parameters()->is_ctype() && return_type()->is_ctype();
    }

    [[nodiscard]] bool contains_pointer_or_func() const noexcept override
    {
        return true;
    }

    void print(std::ostream& stream, SymbolPrintFormat format) const override;

    [[nodiscard]] FuncTypeSymbol* map() override;
};

class BlockTypeSymbol final : public FuncLikeTypeSymbol {
public:
    BlockTypeSymbol()
    {
    }

    BlockTypeSymbol(TupleTypeSymbol* parameters, TypeLikeSymbol* return_type)
        : FuncLikeTypeSymbol(parameters, return_type)
    {
    }

    void print(std::ostream& stream, SymbolPrintFormat format) const override;

    [[nodiscard]] BlockTypeSymbol* map() override;
};

class ConstructedTypeSymbol final : public NamedTypeSymbol {
    TypeDeclarationSymbol* original_;
    std::vector<TypeLikeSymbol*> parameters_;

public:
    explicit ConstructedTypeSymbol(TypeDeclarationSymbol* original);

    [[nodiscard]] bool is_ctype() const noexcept override
    {
        return original_ && original_->is_ctype();
    }

    [[nodiscard]] bool contains_pointer_or_func() const noexcept override
    {
        return original_ && original_->contains_pointer_or_func();
    }

    [[nodiscard]] std::size_t parameter_count() const noexcept override
    {
        return parameters_.size();
    }

    [[nodiscard]] TypeLikeSymbol* parameter(std::size_t index) const override;

    void add_parameter(TypeLikeSymbol* parameter)
    {
        assert(parameter);
        parameters_.push_back(parameter);
    }

    [[nodiscard]] TypeDeclarationSymbol* original() const noexcept override
    {
        return original_;
    }

    [[nodiscard]] bool is_file_level() const noexcept override
    {
        return false;
    }

    [[nodiscard]] InputFile* defining_file() const noexcept override
    {
        return original_->defining_file();
    }

protected:
    void visit_impl(SymbolVisitor& visitor) override;
};

class TypeAliasSymbol final : public NamedTypeSymbol {
    TypeLikeSymbol* target_ = nullptr;

public:
    explicit TypeAliasSymbol(std::string name);

    [[nodiscard]] bool is_ctype() const noexcept override
    {
        return target_ && target_->is_ctype();
    }

    [[nodiscard]] bool contains_pointer_or_func() const noexcept override
    {
        return target_ && target_->contains_pointer_or_func();
    }

    [[nodiscard]] std::size_t parameter_count() const noexcept override
    {
        return 0;
    }

    [[nodiscard]] TypeLikeSymbol* parameter(std::size_t) const override
    {
        assert(false);
        return nullptr;
    }

    [[nodiscard]] TypeLikeSymbol* target() const
    {
        return target_;
    }

    void set_target(TypeLikeSymbol* target)
    {
        // TODO: check the underlying type is identical
        assert(target);
        target_ = target;
    }

    [[nodiscard]] TypeAliasSymbol* original() const noexcept override
    {
        return const_cast<TypeAliasSymbol*>(this);
    }

    [[nodiscard]] TypeAliasSymbol* construct(
        [[maybe_unused]] const std::vector<TypeLikeSymbol*>& arguments) const override
    {
        assert(arguments.empty());
        return const_cast<TypeAliasSymbol*>(this);
    }

    [[nodiscard]] bool is_file_level() const noexcept override
    {
        return true;
    }

    [[nodiscard]] const TypeLikeSymbol& canonical_type() const override;

protected:
    void visit_impl(SymbolVisitor& visitor) override;
    void print(std::ostream& stream, SymbolPrintFormat format) const override;
};

class NonTypeSymbol final : public FileLevelSymbol {
    struct Private {
        explicit Private() = default;
    };

public:
    enum class Kind : std::uint8_t {
        Field,
        Property,
        InstanceVariable,
        GlobalFunction, // NOTE: must have stable address and live forever
        MemberMethod,
        Constructor
    };

    struct ParameterCollection final : SymbolChildren<NonTypeSymbol, ParameterSymbol> {
        explicit ParameterCollection(NonTypeSymbol* self) : SymbolChildren(self)
        {
        }

        [[nodiscard]] ParameterSymbol& get(const std::size_t index) const override
        {
            return self_->parameter(index);
        }

        [[nodiscard]] std::size_t size() const override
        {
            return self_->parameter_count();
        }
    };

    [[nodiscard]] NonTypeSymbol(
        Private, std::string name, const Kind kind, TypeLikeSymbol* return_type, uint16_t modifiers = 0)
        : FileLevelSymbol(std::move(name)), kind_(kind), modifiers_(modifiers), return_type_(return_type)
    {
    }

    [[nodiscard]] NonTypeSymbol(Private, std::string name, std::string getter, std::string setter, uint16_t modifiers)
        : FileLevelSymbol(std::move(name)),
          kind_(Kind::Property),
          modifiers_(modifiers),
          getter_(std::move(getter)),
          setter_(std::move(setter)),
          return_type_(nullptr)
    {
    }

    [[nodiscard]] Kind kind() const noexcept
    {
        return kind_;
    }

    [[nodiscard]] bool is_field() const noexcept
    {
        return kind() == Kind::Field;
    }

    [[nodiscard]] bool is_property() const noexcept
    {
        return kind() == Kind::Property;
    }

    [[nodiscard]] bool is_instance_variable() const noexcept
    {
        return kind() == Kind::InstanceVariable;
    }

    [[nodiscard]] bool is_global_function() const noexcept
    {
        return kind() == Kind::GlobalFunction;
    }

    [[nodiscard]] bool is_member_method() const noexcept
    {
        return kind() == Kind::MemberMethod;
    }

    [[nodiscard]] bool is_constructor() const noexcept
    {
        return kind() == Kind::Constructor;
    }

    [[nodiscard]] bool is_method() const noexcept
    {
        return is_member_method() || is_constructor() || is_global_function();
    }

    [[nodiscard]] uint16_t modifiers() const noexcept
    {
        return modifiers_;
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

    [[nodiscard]] bool is_nullable() const noexcept
    {
        return modifiers_ & ModifierNullable;
    }

    [[nodiscard]] bool is_override() const noexcept
    {
        return modifiers_ & ModifierOverride;
    }

    [[nodiscard]] bool is_optional() const noexcept
    {
        return modifiers_ & ModifierOptional;
    }

    [[nodiscard]] const std::string& getter() const noexcept
    {
        return getter_;
    }

    [[nodiscard]] const std::string& setter() const noexcept
    {
        return setter_;
    }

    [[nodiscard]] bool is_public() const noexcept
    {
        return !(modifiers_ & ModifierAccessMask);
    }

    [[nodiscard]] bool is_protected() const noexcept
    {
        return (modifiers_ & ModifierAccessMask) == ModifierProtected;
    }

    [[nodiscard]] bool is_file_level() const noexcept override
    {
        return is_global_function();
    }

    [[nodiscard]] TypeLikeSymbol* return_type() const noexcept
    {
        return return_type_;
    }

    [[nodiscard]] bool is_bit_field() const noexcept
    {
        return bit_field_size_ != 0xFF;
    }

    [[nodiscard]] bool is_ctype() const noexcept override;

    void set_return_type(TypeLikeSymbol* return_type)
    {
        assert(return_type);
        return_type_ = return_type;
    }

    [[nodiscard]] std::size_t parameter_count() const noexcept
    {
        return parameters_.size();
    }

    [[nodiscard]] ParameterSymbol& parameter(const std::size_t index)
    {
        return parameters_.at(index);
    }

    [[nodiscard]] const ParameterSymbol& parameter(const std::size_t index) const
    {
        return parameters_.at(index);
    }

    [[nodiscard]] ParameterCollection parameters()
    {
        return ParameterCollection(this);
    }

    void add_parameter(std::string name, TypeLikeSymbol* type, bool is_nullable)
    {
        assert(is_method());
        assert(type);
        parameters_.emplace_back(std::move(name), type, is_nullable);
    }

    [[nodiscard]] const std::string& selector_attribute() const noexcept
    {
        return selector_attribute_;
    }

    void rename(std::string_view new_name) override
    {
        assert(!new_name.empty());
        if (selector_attribute_.empty()) {
            selector_attribute_ = name();
        }
        FileLevelSymbol::rename(new_name);
    }

    [[nodiscard]] const std::string& selector() const noexcept
    {
        return selector_attribute_.empty() ? name() : selector_attribute_;
    }

    [[nodiscard]] bool has_internal_linkage() const noexcept
    {
        return modifiers_ & ModifierInternalLinkage;
    }

    void set_bit_field_size(uint8_t size) noexcept
    {
        assert(is_instance()); // A bit-field cannot be a static data member.
        assert(size < 0xFF);
        bit_field_size_ = size;
    }

protected:
    void visit_impl(SymbolVisitor& visitor) override;

private:
    // Only these classes are allowed to create instances of NonTypeSymbol
    friend class TypeDeclarationSymbol;
    friend class TopLevel;

    Kind kind_;
    uint16_t modifiers_;

    // used for Kind::Property
    std::string getter_;
    std::string setter_;

    uint8_t bit_field_size_ = 0xFF; // Non-default value if this is a bit-field
    TypeLikeSymbol* return_type_;
    std::vector<ParameterSymbol> parameters_;
    std::string selector_attribute_;
};

class ParameterSymbol final : public Symbol {
    TypeLikeSymbol* type_;
    const bool is_nullable_;

public:
    ParameterSymbol(std::string name, TypeLikeSymbol* type, bool is_nullable)
        : Symbol(std::move(name)), type_(type), is_nullable_(is_nullable)
    {
    }

    [[nodiscard]] TypeLikeSymbol* type() const
    {
        return type_;
    }

    void set_type(TypeLikeSymbol* type)
    {
        assert(type);
        type_ = type;
    }

    [[nodiscard]] bool is_nullable() const noexcept
    {
        return is_nullable_;
    }
};

[[nodiscard]] TypeLikeSymbol& pointer(TypeLikeSymbol& pointee);

} // namespace objcgen

#endif // SYMBOL_H
