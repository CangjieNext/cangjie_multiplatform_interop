// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "Symbol.h"

#include <iostream>

#include "Logging.h"
#include "Mappings.h"
#include "Mode.h"
#include "Package.h"
#include "Universe.h"

namespace objcgen {

std::ostream& operator<<(std::ostream& stream, const KeywordEscaper& op)
{
    // Do not include keywords common for Cangjie and C/Objective-C
    static constexpr const char* cangjieKeywords[] = {
        "as", "Bool",
        //"break",
        //"case",
        "catch", "class",
        //"const",
        //"continue",
        //"do",
        //"else",
        //"enum",
        "extend", "false", "finally", "Float16", "Float32", "Float64",
        //"for",
        "foreign", "from", "func", "handle",
        //"if",
        "import", "in", "init", "inout", "Int16", "Int32", "Int64", "Int8", "interface", "IntNative", "is", "let",
        "macro", "main", "match", "mut", "Nothing", "operator", "package", "perform", "prop", "quote", "resume",
        //"return",
        "Rune", "spawn",
        //"static",
        //"struct",
        "super", "synchronized", "This", "this", "throw", "true", "try", "type", "UInt16", "UInt32", "UInt64", "UInt8",
        "UIntNative", "Unit", "unsafe", "var", "where",
        //"while",
    };
    auto e = std::cend(cangjieKeywords);
    if (std::find(std::cbegin(cangjieKeywords), e, op.name) != e) {
        stream << '`' << op.name << '`';
    } else {
        stream << op.name;
    }
    return stream;
}

Symbol::Symbol(std::string name) noexcept : name_(std::move(name))
{
}

void Symbol::print(std::ostream& stream, [[maybe_unused]] SymbolPrintFormat format)
{
    stream << escape_keyword(name_);
}

std::ostream& operator<<(std::ostream& stream, const SymbolPrinter& symbolPrinter)
{
    symbolPrinter.symbol_.print(stream, symbolPrinter.format_);
    return stream;
}

void SymbolVisitor::recurse(FileLevelSymbol* value)
{
    value->visit_impl(*this);
}

void NamedTypeSymbol::rename(const std::string_view new_name)
{
    const auto old_name = name();
    TypeLikeSymbol::rename(new_name);
    Universe::get().process_rename(this, old_name);
}

template <class UnaryPred>
bool NamedTypeSymbol::is_recursively(UnaryPred cond) const noexcept(noexcept(cond(std::declval<NamedTypeSymbol>())))
{
    if (cond(*this)) {
        return true;
    }
    if (const auto* alias = dynamic_cast<const TypeAliasSymbol*>(this)) {
        if (const auto* target = dynamic_cast<const NamedTypeSymbol*>(alias->target())) {
            return target->is_recursively(cond);
        }
    }
    return false;
}

bool NamedTypeSymbol::is_objc_reference() const noexcept
{
    return is_recursively([](const auto& symbol) {
        switch (symbol.kind_) {
            case Kind::Interface:
            case Kind::Protocol:
                return true;
            default:
                return false;
        }
    });
}

static void print_type_arguments(std::ostream& stream, const NamedTypeSymbol& type_symbol, SymbolPrintFormat format)
{
    auto no_type_arguments = format != SymbolPrintFormat::Raw;
    if (no_type_arguments) {
        stream << "/*";
    }
    stream << "<";
    auto count = type_symbol.parameter_count();
    assert(count);
    for (std::size_t i = 0; i < count; ++i) {
        if (i != 0) {
            stream << ", ";
        }

        auto* parameter = type_symbol.parameter(i);
        assert(parameter);
        stream << raw(*parameter);
    }
    stream << '>';
    if (no_type_arguments) {
        stream << "*/";
    }
}

void NamedTypeSymbol::print(std::ostream& stream, SymbolPrintFormat format)
{
    auto name = this->name();
    if (kind_ == Kind::Primitive) {
        stream << name;
    } else {
        stream << escape_keyword(name);
        if (parameter_count()) {
            print_type_arguments(stream, *this, format);
        }
    }
}

TypeLikeSymbol* NamedTypeSymbol::map()
{
    if (auto* mapping = this->mapping())
        return mapping->map(this);

    if (const auto parameter_count = this->parameter_count(); parameter_count) {
        std::vector<TypeLikeSymbol*> arguments;
        arguments.reserve(parameter_count);
        auto changed = false;

        for (std::size_t i = 0; i < parameter_count; ++i) {
            auto* old_parameter = this->parameter(i);
            auto* new_parameter = old_parameter->map();
            arguments.push_back(new_parameter);
            changed = changed || new_parameter != old_parameter;
        }

        if (changed) {
            return construct(arguments);
        }
    }

    return this;
}

NamedTypeSymbol* NamedTypeSymbol::construct(const std::vector<TypeLikeSymbol*>& arguments)
{
    const auto parameter_count = this->parameter_count();
    assert(parameter_count == arguments.size());

    if (parameter_count) {
        auto changed = false;

        for (std::size_t i = 0; i < parameter_count; ++i) {
            const auto* old_parameter = this->parameter(i);
            const auto* new_parameter = arguments.at(i);
            changed = changed || new_parameter != old_parameter;
        }

        if (changed) {
            auto* original = this->original();
            assert(original);
            auto* original_type = dynamic_cast<TypeDeclarationSymbol*>(original);
            assert(original_type);
            auto* result = new ConstructedTypeSymbol(original_type);
            const auto& cangjie_package_name = original_type->cangjie_package_name();
            if (!cangjie_package_name.empty()) {
                result->set_cangjie_package_name(cangjie_package_name);
            }
            for (auto* argument : arguments) {
                result->add_parameter(argument);
            }
            return result;
        }
    }

    return this;
}

TypeLikeSymbol& EnumDeclarationSymbol::underlying_type() const noexcept
{
    assert(underlying_type_);
    return *underlying_type_;
}

void UnexposedTypeSymbol::print(std::ostream& stream, SymbolPrintFormat format)
{
    canonical_type().print(stream, format);
    stream << " /*" << name() << "*/";
}

void UnexposedTypeSymbol::print_default_value(
    std::ostream& stream, const NonTypeSymbol& symbol, SymbolPrintFormat format, TypeLikeSymbol& type_alias)
{
    canonical_type().print_default_value(stream, symbol, format, type_alias);
}

static bool is_ctype_by_default(NamedTypeSymbol::Kind kind, std::string_view name)
{
    switch (kind) {
        case NamedTypeSymbol::Kind::Primitive:
        case NamedTypeSymbol::Kind::Enum:

        // Empty structures are CType.  If afterwards a non-CType member is added,
        // `is_ctype_` will be set to `false`.
        case NamedTypeSymbol::Kind::Struct:
        case NamedTypeSymbol::Kind::Union:
            return name != "ObjCPointer" && name != "ObjCFunc" && name != "ObjCBlock";
        default:
            return false;
    }
}

TypeDeclarationSymbol::TypeDeclarationSymbol(const Kind kind, std::string name)
    : NamedTypeSymbol(kind, std::move(name)), is_ctype_(is_ctype_by_default(kind, this->name()))
{
}

TypeParameterSymbol* TypeDeclarationSymbol::parameter(const std::size_t index) const
{
    return parameters_.at(index).get();
}

void TypeDeclarationSymbol::member_remove(size_t index)
{
    auto it = members_.begin();
    std::advance(it, index);
    switch (kind()) {
        case Kind::Struct:
        case Kind::Union:
            assert(it->kind() == NonTypeSymbol::Kind::Field);
            {
                auto removing_ctype = it->return_type()->is_ctype();
                members_.erase(it);
                if (!removing_ctype) {
                    is_ctype_ = all_of_members([](const auto& member) { return member.return_type()->is_ctype(); });
                }
                contains_pointer_or_func_ =
                    any_of_members([](const auto& member) { return member.return_type()->contains_pointer_or_func(); });
            }
            break;
        default:
            members_.erase(it);
            break;
    }
}

template <class Pred>
bool TypeDeclarationSymbol::all_of_members(Pred cond) const noexcept(noexcept(cond(std::declval<NonTypeSymbol>())))
{
    return std::all_of(members_.cbegin(), members_.cend(), [cond](const auto& member) { return cond(member); });
}

template <class Pred>
bool TypeDeclarationSymbol::any_of_members(Pred cond) const noexcept(noexcept(cond(std::declval<NonTypeSymbol>())))
{
    return std::any_of(members_.cbegin(), members_.cend(), [cond](const auto& member) { return cond(member); });
}

void TypeDeclarationSymbol::add_parameter(std::string name)
{
    for ([[maybe_unused]] const auto& parameter : parameters_) {
        assert(parameter->name() != name);
    }

    parameters_.push_back(std::make_unique<TypeParameterSymbol>(TypeParameterSymbol::Private(), std::move(name)));
}

NonTypeSymbol& TypeDeclarationSymbol::add_member_method(
    std::string name, TypeLikeSymbol* return_type, uint16_t modifiers)
{
    // No clash detection, otherwise might assert on method overloads

    assert(kind() == Kind::Interface || kind() == Kind::Protocol || kind() == Kind::TopLevel);
    return members_.emplace_back(
        NonTypeSymbol::Private(), std::move(name), NonTypeSymbol::Kind::MemberMethod, return_type, modifiers);
}

NonTypeSymbol& TypeDeclarationSymbol::add_constructor(std::string name, TypeLikeSymbol* return_type)
{
    assert(kind() == Kind::Interface || kind() == Kind::Protocol);
    return members_.emplace_back(
        NonTypeSymbol::Private(), std::move(name), NonTypeSymbol::Kind::Constructor, return_type);
}

NonTypeSymbol& TypeDeclarationSymbol::add_field(std::string name, TypeLikeSymbol* type, uint16_t modifiers)
{
    assert(kind() == Kind::Struct || kind() == Kind::Union);

    // Only bit-fields can be unnamed
    assert(!name.empty() || (modifiers & ModifierBitField));

    // And once it is named, no other field with this name should exist
    assert(name.empty() ||
        all_of_members([&name](const auto& member) { return !member.is_field() || member.name() != name; }));

    auto& member =
        members_.emplace_back(NonTypeSymbol::Private(), std::move(name), NonTypeSymbol::Kind::Field, type, modifiers);
    if (is_ctype_ && !member.return_type()->is_ctype()) {
        is_ctype_ = false;
    }
    if (member.return_type()->contains_pointer_or_func()) {
        contains_pointer_or_func_ = true;
    }
    return member;
}

NonTypeSymbol& TypeDeclarationSymbol::add_instance_variable(std::string name, TypeLikeSymbol* type, uint16_t modifiers)
{
    assert(kind() == Kind::Interface);
    assert(all_of_members([&name](const auto& member) { return member.name() != name; }));

    auto& ivar = members_.emplace_back(
        NonTypeSymbol::Private(), std::move(name), NonTypeSymbol::Kind::InstanceVariable, type, modifiers);
    if (is_ctype_ && !ivar.return_type()->is_ctype()) {
        is_ctype_ = false;
    }
    if (ivar.return_type()->contains_pointer_or_func()) {
        contains_pointer_or_func_ = true;
    }
    return ivar;
}

EnumConstantSymbol& EnumDeclarationSymbol::add_constant(std::string name, const std::array<uint64_t, 2>& value)
{
    assert(std::all_of(
        constants_.begin(), constants_.end(), [name](const auto& constant) { return constant.name() != name; }));
    return constants_.emplace_back(std::move(name), value);
}

void EnumDeclarationSymbol::print_default_value(
    std::ostream& stream, const NonTypeSymbol& symbol, SymbolPrintFormat format, TypeLikeSymbol& type_alias)
{
    assert(underlying_type_);
    underlying_type_->canonical_type().print_default_value(stream, symbol, format, type_alias);
}

void PrimitiveTypeSymbol::print_default_value(std::ostream& stream, [[maybe_unused]] const NonTypeSymbol& symbol,
    [[maybe_unused]] SymbolPrintFormat format, [[maybe_unused]] TypeLikeSymbol& type_alias)
{
    switch (category_) {
        case PrimitiveTypeCategory::Boolean:
            stream << "false";
            break;
        case PrimitiveTypeCategory::SignedInteger:
        case PrimitiveTypeCategory::UnsignedInteger:
            stream << '0';
            break;
        case PrimitiveTypeCategory::FloatingPoint:
            stream << "0.0";
            break;
        default:
            assert(category_ == PrimitiveTypeCategory::Unit);
            stream << "()";
            break;
    }
}

NonTypeSymbol& TypeDeclarationSymbol::add_property(
    std::string name, std::string getter, std::string setter, uint16_t modifiers)
{
    assert(kind() == Kind::Interface || kind() == Kind::Protocol);

    return members_.emplace_back(
        NonTypeSymbol::Private(), std::move(name), std::move(getter), std::move(setter), modifiers);
}

void TypeDeclarationSymbol::add_base(TypeDeclarationSymbol& base)
{
    bases_.push_back(&base);
}

const std::string& FileLevelSymbol::cangjie_package_name() const
{
    if (!cangjie_package_name_.empty()) {
        assert(!output_file_);
        return cangjie_package_name_;
    }
    if (!output_file_) {
        return cangjie_package_name_;
    }
    auto* package = output_file_->package();
    assert(package);
    return package->cangjie_name();
}

Package* FileLevelSymbol::package() const
{
    const auto* file = package_file();
    return file ? file->package() : nullptr;
}

void FileLevelSymbol::set_definition_location(const Location& location)
{
    assert(!input_file_);
    input_file_ = &inputs[location.file_];
    location_ = location.pos_;
    input_file_->add_symbol(this);
}

void FileLevelSymbol::set_package_file(PackageFile* package_file)
{
    assert(cangjie_package_name_.empty());
    assert(!output_file_);
    assert(package_file);
    assert(this->is_file_level());
    output_file_ = package_file;
}

void FileLevelSymbol::set_cangjie_package_name(std::string cangjie_package_name) noexcept
{
    assert(cangjie_package_name_.empty());
    assert(!output_file_);
    assert(!cangjie_package_name.empty());
    cangjie_package_name_ = std::move(cangjie_package_name);
}

bool FileLevelSymbol::add_reference(FileLevelSymbol& symbol)
{
    assert(&symbol != this);
    assert(symbol.is_file_level());
    assert(is_file_level());
    return references_symbols_.insert(&symbol).second;
}

static bool referencing_packages_detailed_info() noexcept
{
    return verbosity > LogLevel::WARNING;
}

void FileLevelSymbol::add_referencing_package(const Package& package)
{
    ++number_of_referencing_packages_;
    if (referencing_packages_detailed_info()) {
        referencing_packages_.insert(&package);
    }
}

size_t FileLevelSymbol::number_of_referencing_packages() const noexcept
{
    return referencing_packages_detailed_info() ? referencing_packages_.size() : number_of_referencing_packages_;
}

void FileLevelSymbol::print_referencing_packages_info() const
{
    if (referencing_packages_detailed_info()) {
        std::cerr << ":\n";
        for (const auto* package : referencing_packages_) {
            std::cerr << "* " << package->cangjie_name() << std::endl;
        }
    } else {
        std::cerr << ". Specify -v for more detailed information" << std::endl;
    }
}

void TypeLikeSymbol::print_default_value(std::ostream& stream, [[maybe_unused]] const NonTypeSymbol& symbol,
    [[maybe_unused]] SymbolPrintFormat format, TypeLikeSymbol& type_alias)
{
    stream << emit_cangjie(type_alias) << "()";
}

TypeParameterSymbol::TypeParameterSymbol(Private, std::string name) : TypeLikeSymbol(std::move(name))
{
}

static void print_tricky_default_value(std::ostream& stream, std::string_view type_name, bool is_nullable)
{
    if (is_nullable) {
        stream << "None";
    } else {
        // The dirty trick is applied for printing default values of:
        // - Interface types -- instances of the interface type cannot be created.
        // - @ObjCMirror classes -- they do not have a primary constructor.
        stream << "Option<" << type_name << ">.None.getOrThrow()";
    }
}

void TypeParameterSymbol::print(std::ostream& stream, SymbolPrintFormat format)
{
    if (format == SymbolPrintFormat::Raw) {
        stream << name();
    } else {
        stream << "ObjCId /*" << name() << "*/";
    }
}

void TypeParameterSymbol::print_default_value(std::ostream& stream, const NonTypeSymbol& symbol,
    [[maybe_unused]] SymbolPrintFormat format, [[maybe_unused]] TypeLikeSymbol& type_alias)
{
    print_tricky_default_value(stream, "ObjCId", symbol.is_nullable());
}

void NarrowedTypeParameterSymbol::print(std::ostream& stream, SymbolPrintFormat format)
{
    if (format == SymbolPrintFormat::Raw) {
        stream << name() << '<' << protocol_name_ << '>';
    } else {
        stream << protocol_name_ << " /*" << name() << '<' << protocol_name_ << ">*/";
    }
}

void PointerTypeSymbol::print(std::ostream& stream, SymbolPrintFormat format)
{
    if (!is_ctype() || format == SymbolPrintFormat::EmitCangjieStrict) {
        stream << "ObjCPointer<";
        pointee_->print(stream, SymbolPrintFormat::EmitCangjieStrict);
    } else {
        stream << "CPointer<";
        pointee_->print(stream, format);
    }
    stream << '>';
}

[[nodiscard]] PointerTypeSymbol* PointerTypeSymbol::map()
{
    auto* new_pointee = pointee_->map();
    return new_pointee == pointee_ ? this : new PointerTypeSymbol(*new_pointee);
}

void PointerTypeSymbol::print_default_value(std::ostream& stream, [[maybe_unused]] const NonTypeSymbol& symbol,
    SymbolPrintFormat format, [[maybe_unused]] TypeLikeSymbol& type_alias)
{
    print(stream, format);
    stream << (!is_ctype() || format == SymbolPrintFormat::EmitCangjieStrict ? "(CPointer<Unit>())" : "()");
}

void VArraySymbol::print(std::ostream& stream, SymbolPrintFormat format)
{
    stream << name() << '<';
    element_type_->print(stream, format);
    stream << ", $" << size_ << '>';
}

[[nodiscard]] VArraySymbol* VArraySymbol::map()
{
    auto* new_element_type = element_type_->map();
    return new_element_type == element_type_ ? this : new VArraySymbol(*new_element_type, size_);
}

void VArraySymbol::print_default_value(
    std::ostream& stream, const NonTypeSymbol& symbol, SymbolPrintFormat format, TypeLikeSymbol& type_alias)
{
    stream << '[';
    if (size_) {
        element_type_->print_default_value(stream, symbol, format, type_alias);
        for (size_t i = 1; i < size_; ++i) {
            stream << ", ";
            element_type_->print_default_value(stream, symbol, format, type_alias);
        }
    }
    stream << ']';
}

void TypeDeclarationSymbol::visit_impl(SymbolVisitor& visitor)
{
    // It could make sense to analyze if infinite recursion is possible her.  With
    // CRTP for example.
    if (const auto count = this->parameter_count(); count) {
        for (std::size_t i = 0; i < count; ++i) {
            visitor.visit(this, this->parameter(i), SymbolProperty::TypeArgument);
        }
    }
    if (const auto count = this->base_count(); count) {
        for (std::size_t i = 0; i < count; ++i) {
            visitor.visit(this, this->base(i), SymbolProperty::Base);
        }
    }
    if (const auto count = this->member_count(); count) {
        for (std::size_t i = 0; i < count; ++i) {
            visitor.visit(this, &this->member(i), SymbolProperty::Member);
        }
    }
}

void TypeDeclarationSymbol::print_default_value(
    std::ostream& stream, const NonTypeSymbol& symbol, SymbolPrintFormat format, TypeLikeSymbol& type_alias)
{
    switch (kind()) {
        case Kind::Interface:
        case Kind::Protocol:
            print_tricky_default_value(stream, type_alias.name(), symbol.is_nullable());
            break;
        default:
            TypeLikeSymbol::print_default_value(stream, symbol, format, type_alias);
            break;
    }
}

TupleTypeSymbol::TupleTypeSymbol() : TypeLikeSymbol(""), is_ctype_(true), contains_pointer_or_func_(false)
{
}

TupleTypeSymbol::TupleTypeSymbol(std::vector<TypeLikeSymbol*> items)
    : TypeLikeSymbol(""),
      items_(std::move(items)),
      is_ctype_(std::all_of(items_.cbegin(), items_.cend(), [](const auto* item) { return item->is_ctype(); })),
      contains_pointer_or_func_(std::any_of(
          items_.cbegin(), items_.cend(), [](const auto* item) { return item->contains_pointer_or_func(); }))
{
}

TypeLikeSymbol* TupleTypeSymbol::item(const std::size_t index) const
{
    return items_.at(index);
}

void TupleTypeSymbol::print(std::ostream& stream, SymbolPrintFormat format)
{
    stream << '(';
    for (std::size_t i = 0; i < items_.size(); ++i) {
        if (i != 0) {
            stream << ", ";
        }
        items_[i]->print(stream, format);
    }
    stream << ')';
}

TupleTypeSymbol* TupleTypeSymbol::map()
{
    std::vector<TypeLikeSymbol*> new_items;
    auto changed = false;
    const std::size_t count = item_count();
    for (std::size_t i = 0; i < count; i++) {
        auto* item = this->item(i);
        auto* new_item = item->map();
        new_items.push_back(new_item);
        changed = changed || item != new_item;
    }
    return changed ? new TupleTypeSymbol(new_items) : this;
}

void TupleTypeSymbol::visit_impl(SymbolVisitor& visitor)
{
    if (const auto count = this->item_count(); count) {
        for (std::size_t i = 0; i < count; ++i) {
            visitor.visit(this, this->item(i), SymbolProperty::TupleItem);
        }
    }
}

FuncLikeTypeSymbol::FuncLikeTypeSymbol() : TypeLikeSymbol("")
{
}

FuncLikeTypeSymbol::FuncLikeTypeSymbol(TupleTypeSymbol* parameters, TypeLikeSymbol* return_type)
    : TypeLikeSymbol(""), parameters_(parameters), return_type_(return_type)
{
}

void FuncLikeTypeSymbol::do_print(std::ostream& stream, std::string_view name, SymbolPrintFormat format) const
{
    stream << name << '<';
    parameters_->print(stream, format);
    stream << " -> ";
    return_type_->print(stream, format);
    stream << '>';
}

void FuncTypeSymbol::print(std::ostream& stream, SymbolPrintFormat format)
{
    if (!is_ctype() || format == SymbolPrintFormat::EmitCangjieStrict) {
        do_print(stream, "ObjCFunc", SymbolPrintFormat::EmitCangjieStrict);
    } else {
        do_print(stream, "CFunc", format);
    }
}

void BlockTypeSymbol::print(std::ostream& stream, SymbolPrintFormat)
{
    do_print(stream, "ObjCBlock", SymbolPrintFormat::EmitCangjieStrict);
}

FuncTypeSymbol* FuncTypeSymbol::map()
{
    auto* parameters = this->parameters();
    auto* return_type = this->return_type();
    auto* new_parameters = parameters->map();
    auto* new_return_type = return_type->map();
    const auto changed = parameters != new_parameters || return_type != new_return_type;
    return changed ? new FuncTypeSymbol(new_parameters, new_return_type) : this;
}

void FuncTypeSymbol::print_default_value(std::ostream& stream, [[maybe_unused]] const NonTypeSymbol& symbol,
    SymbolPrintFormat format, [[maybe_unused]] TypeLikeSymbol& type_alias)
{
    print(stream, format);
    stream << (!is_ctype() || format == SymbolPrintFormat::EmitCangjieStrict ? "(CPointer<CFunc<() -> Unit>>())"
                                                                             : "(CPointer<Unit>())");
}

BlockTypeSymbol* BlockTypeSymbol::map()
{
    auto* parameters = this->parameters();
    auto* return_type = this->return_type();
    auto* new_parameters = parameters->map();
    auto* new_return_type = return_type->map();
    const auto changed = parameters != new_parameters || return_type != new_return_type;
    return changed ? new BlockTypeSymbol(new_parameters, new_return_type) : this;
}

void BlockTypeSymbol::print_default_value(std::ostream& stream, [[maybe_unused]] const NonTypeSymbol& symbol,
    SymbolPrintFormat format, [[maybe_unused]] TypeLikeSymbol& type_alias)
{
    print(stream, format);
    stream << "(unsafe { ";
    do_print(stream, "zeroValue", format);
    stream << "() })";
}

void FuncLikeTypeSymbol::visit_impl(SymbolVisitor& visitor)
{
    visitor.visit(this, this->parameters(), SymbolProperty::FunctionParametersTuple);
    visitor.visit(this, this->return_type(), SymbolProperty::FunctionReturnType);
}

ConstructedTypeSymbol::ConstructedTypeSymbol(TypeDeclarationSymbol* original)
    : NamedTypeSymbol(original->kind(), std::string(original->name())), original_(original)
{
    assert(original->original() == original);
}

TypeLikeSymbol* ConstructedTypeSymbol::parameter(const std::size_t index) const
{
    return parameters_.at(index);
}

void ConstructedTypeSymbol::print_default_value(
    std::ostream& stream, const NonTypeSymbol& symbol, SymbolPrintFormat format, TypeLikeSymbol& type_alias)
{
    assert(original_);
    return original_->print_default_value(stream, symbol, format, type_alias);
}

void ConstructedTypeSymbol::visit_impl(SymbolVisitor& visitor)
{
    if (const auto count = this->parameter_count(); count) {
        for (std::size_t i = 0; i < count; ++i) {
            visitor.visit(this, this->parameter(i), SymbolProperty::TypeArgument);
        }
    }
}

TypeAliasSymbol::TypeAliasSymbol(std::string name) : NamedTypeSymbol(Kind::TypeDef, std::move(name))
{
}

void TypeAliasSymbol::visit_impl(SymbolVisitor& visitor)
{
    visitor.visit(this, this->target(), SymbolProperty::AliasTarget);
}

void TypeAliasSymbol::print(std::ostream& stream, SymbolPrintFormat format)
{
    auto* target = this->target();
    if (target && name() == target->name()) {
        // typedef struct S S;
        target->print(stream, format);
        return;
    }
    if (mode != Mode::EXPERIMENTAL && format == SymbolPrintFormat::EmitCangjieStrict) {
        auto& canonical_type = this->canonical_type();
        if (canonical_type.is_ctype() && canonical_type.contains_pointer_or_func()) {
            stream << emit_cangjie_strict(canonical_type) << " /*" << emit_cangjie(*this) << "*/";
            return;
        }
    }
    NamedTypeSymbol::print(stream, format);
}

void TypeAliasSymbol::print_default_value(
    std::ostream& stream, const NonTypeSymbol& symbol, SymbolPrintFormat format, TypeLikeSymbol& type_alias)
{
    canonical_type().print_default_value(stream, symbol, format, type_alias);
}

TypeLikeSymbol& TypeAliasSymbol::canonical_type()
{
    auto* target = this->target();
    assert(target);
    return target->canonical_type();
}

bool NonTypeSymbol::is_ctype() const noexcept
{
    auto n = parameter_count();
    for (size_t i = 0; i < n; ++i) {
        const auto& p = parameter(i);
        if (!p.type()->is_ctype()) {
            return false;
        }
    }
    return return_type_->is_ctype();
}

void NonTypeSymbol::visit_impl(SymbolVisitor& visitor)
{
    if (const auto count = this->parameter_count(); count) {
        for (std::size_t i = 0; i < count; ++i) {
            visitor.visit(this, this->parameter(i).type(), SymbolProperty::ParameterType);
        }
    }

    visitor.visit(this, this->return_type(), SymbolProperty::ReturnType);
}

TypeLikeSymbol& pointer(TypeLikeSymbol& pointee)
{
    // C pointer to anything other than function is converted to PointerTypeSymbol.
    // Pointer-to-function is converted to FuncTypeSymbol.
    auto* func = dynamic_cast<FuncTypeSymbol*>(&pointee);
    if (func) {
        return *func;
    }
    return *new PointerTypeSymbol(pointee);
}

} // namespace objcgen
