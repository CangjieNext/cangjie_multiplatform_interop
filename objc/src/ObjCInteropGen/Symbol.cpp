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
#include "PrintUtils.h"
#include "SymbolVisitor.h"
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

void Symbol::rename(std::string_view new_name)
{
    assert(!new_name.empty());
    name_ = new_name;
}

void Symbol::print(std::ostream& stream, [[maybe_unused]] PrintFormat format) const
{
    stream << escape_keyword(name_);
}

void FileLevelSymbol::set_definition_location(const Location& location)
{
    assert(!input_file_);
    input_file_ = &inputs[location.file_];
    location_ = location.pos_;
    input_file_->add_symbol(*this);
}

bool FileLevelSymbol::add_reference(FileLevelSymbol& symbol)
{
    assert(&symbol != this);
    assert(symbol.is_file_level());
    assert(is_file_level());
    return references_symbols_.insert(&symbol).second;
}

void FileLevelSymbol::set_package_file(PackageFile& package_file) noexcept
{
    assert(cangjie_package_name_.empty());
    assert(!output_file_);
    assert(this->is_file_level());
    output_file_ = &package_file;
}

[[nodiscard]] static bool referencing_packages_detailed_info() noexcept
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

const std::string& FileLevelSymbol::cangjie_package_name() const noexcept
{
    if (!cangjie_package_name_.empty()) {
        assert(!output_file_);
        return cangjie_package_name_;
    }
    return output_file_ ? output_file_->package().cangjie_name() : cangjie_package_name_;
}

Package* FileLevelSymbol::package() const noexcept
{
    const auto* file = package_file();
    return file ? &file->package() : nullptr;
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

// Applicable only for symbols with the same defining file
[[nodiscard]] bool operator<(const FileLevelSymbol& symbol1, const FileLevelSymbol& symbol2) noexcept
{
    assert(symbol1.input_file_);
    assert(symbol1.input_file_ == symbol2.input_file_);
    return symbol1.location_ < symbol2.location_;
}

void FileLevelSymbol::set_cangjie_package_name(std::string cangjie_package_name) noexcept
{
    assert(cangjie_package_name_.empty());
    assert(!output_file_);
    assert(!cangjie_package_name.empty());
    cangjie_package_name_ = std::move(cangjie_package_name);
}

[[nodiscard]] static Type::Kind get_kind(const TypeLikeSymbol& type_symbol)
{
    if (type_symbol.is<TypeParameterSymbol>()) {
        return Type::Kind::TypeParam;
    }
    auto& universe = Universe::get();
    return &type_symbol == &universe.pointer() ? Type::Kind::Pointer
        : &type_symbol == &universe.func()     ? Type::Kind::Function
        : &type_symbol == &universe.block()    ? Type::Kind::Block
                                               : Type::Kind::Named;
}

Type::Type(TypeLikeSymbol& symbol, std::vector<Type>&& parameters, Nullability nullability) noexcept
    : kind_(get_kind(symbol)),
      symbol_(&symbol),
      parameters_(std::move(parameters)),
      nullability_(init_nullability(nullability))
{
}

Type::Type(TypeLikeSymbol& symbol, Nullability nullability) noexcept
    : kind_(get_kind(symbol)), symbol_(&symbol), nullability_(init_nullability(nullability))
{
}

Type::Type(Type varray_element_type, size_t varray_size)
    : kind_(Type::Kind::VArray),
      symbol_(&Universe::get().varray()),
      parameters_{varray_element_type},
      varray_size_(varray_size)
{
}

bool Type::is_unit() const noexcept
{
    assert(symbol_);
    return symbol_->is_unit();
}

const TypeLikeSymbol& Type::symbol() const noexcept
{
    assert(symbol_);
    return *symbol_;
}

TypeLikeSymbol& Type::symbol() noexcept
{
    assert(symbol_);
    return *symbol_;
}

const std::string& Type::name() const noexcept
{
    assert(symbol_);
    return symbol_->name();
}

const TypeDeclarationSymbol& Type::actual_protocol() const noexcept
{
    // Probably, it would be good to return the least common ancestor of all
    // constraints here, not just `id`.
    assert(kind_ == Kind::TypeParam);
    return parameters_.size() == 1 ? parameters_.front().symbol().as<TypeDeclarationSymbol>() : Universe::get().id();
}

void Type::set_nullability(Nullability nullability) noexcept
{
    assert(kind_ == Kind::Named || kind_ == Kind::TypeParam);
    nullability_ = nullability;
}

const Type& Type::varray_element_type() const noexcept
{
    assert(kind_ == Kind::VArray);
    assert(parameters_.size() == 1);
    return parameters_.front();
}

void Type::visit_impl(SymbolVisitor& visitor) const
{
    switch (kind_) {
        case Kind::Unit:
            break;
        case Kind::Named:
        case Kind::Function:
        case Kind::Block:
            assert(symbol_);
            for (const auto& param : parameters_) {
                visitor.visit_type_argument(*symbol_, param);
            }
            break;
        case Kind::Pointer:
        case Kind::VArray:
            assert(symbol_);
            assert(parameters_.size() == 1);
            visitor.visit_type_argument(*symbol_, parameters_.front());
            break;
        default:
            break;
    }
}

bool Type::is_ctype() const noexcept
{
    switch (kind_) {
        case Kind::Named:
            assert(symbol_);
            return symbol_->is_ctype();
        case Kind::VArray:
            return varray_element_type().is_ctype();
        case Kind::Pointer:
            assert(parameters_.size() == 1);
            return parameters_.front().is_ctype();
        case Kind::Function:
            return std::none_of(
                parameters_.begin(), parameters_.end(), [](const auto& parameter) { return !parameter.is_ctype(); });
        case Kind::Block:
        case Kind::TypeParam:
            return false;
        default:
            assert(kind_ == Kind::Unit || kind_ == Kind::Unexposed);
            return true;
    }
}

bool Type::contains_pointer_or_func() const noexcept
{
    switch (kind_) {
        case Kind::Named:
            assert(symbol_);
            return symbol_->contains_pointer_or_func();
        case Kind::VArray:
            return varray_element_type().contains_pointer_or_func();
        case Kind::Pointer:
        case Kind::Function:
            return true;
        case Kind::Block:
            return std::any_of(parameters_.begin(), parameters_.end(),
                [](const auto& parameter) { return parameter.contains_pointer_or_func(); });
        default:
            assert(kind_ == Kind::Unit || kind_ == Kind::TypeParam || kind_ == Kind::Unexposed);
            return false;
    }
}

Type Type::canonical_type() const
{
    const auto* alias = dynamic_cast<const TypeAliasSymbol*>(symbol_);
    auto result = alias ? alias->canonical_type() : *this;
    if (is_cj_option()) {
        result.set_nullability(Nullability::Nullable);
    }
    return result;
}

const TypeLikeSymbol& Type::canonical_type_symbol() const noexcept
{
    const auto* alias = dynamic_cast<const TypeAliasSymbol*>(symbol_);
    return alias ? alias->canonical_type_symbol() : symbol();
}

bool Type::is_optionable_reference() const noexcept
{
    return symbol_ && symbol_->is_optionable_reference();
}

bool Type::is_cj_direct_option() const noexcept
{
    if (is_optionable_reference()) {
        return nullability_ != Nullability::Nonnull;
    }
    if (kind_ != Kind::Named) {
        return false;
    }
    assert(symbol_);
    const auto* alias = dynamic_cast<const TypeAliasSymbol*>(symbol_);
    return alias && alias->canonical_type_symbol().is_optionable_reference() && nullability_ == Nullability::Nullable &&
        !alias->target().is_cj_option();
}

bool Type::is_cj_option() const noexcept
{
    if (is_optionable_reference()) {
        return nullability_ != Nullability::Nonnull;
    }
    if (kind_ != Kind::Named) {
        return false;
    }
    assert(symbol_);
    const auto* alias = dynamic_cast<const TypeAliasSymbol*>(symbol_);
    return alias && alias->canonical_type_symbol().is_optionable_reference() &&
        (nullability_ == Nullability::Nullable || alias->target().is_cj_option());
}

void Type::map()
{
    switch (kind_) {
        case Kind::Unit:
            break;
        case Kind::Named:
        case Kind::Function:
        case Kind::Block:
            assert(symbol_);
            symbol_ = &symbol_->map();
            for (auto& parameter : parameters_) {
                parameter.map();
            }
            break;
        case Kind::Pointer:
        case Kind::VArray:
            assert(parameters_.size() == 1);
            parameters_.front().map();
            break;
        default:
            assert(symbol_);
            symbol_ = &symbol_->map();
            break;
    }
}

static void print_raw_type_parameter(std::ostream& stream, const Type& type_param)
{
    assert(type_param.kind() == Type::Kind::TypeParam);
    stream << type_param.name();
    const auto& constraints = type_param.parameters();
    if (!constraints.empty()) {
        stream << '<';
        print_list(stream, constraints, [](auto& stream, const auto& constraint) { stream << constraint.name(); });
        stream << '>';
    }
}

void Type::print(std::ostream& stream, PrintFormat format) const
{
    if (is_cj_direct_option()) {
        stream << '?';
    }
    switch (kind_) {
        case Kind::Unit:
            break;
        case Kind::Named: {
            assert(symbol_);
            symbol_->print(stream, format);
            if (!parameters_.empty()) {
                auto no_type_arguments = format != PrintFormat::Raw;
                if (no_type_arguments) {
                    stream << "/*";
                }
                stream << '<';
                print_list(stream, parameters_, [](auto& stream, const auto& parameter) { stream << raw(parameter); });
                stream << '>';
                if (no_type_arguments) {
                    stream << "*/";
                }
            }
            break;
        }
        case Kind::Pointer: {
            assert(parameters().size() == 1);
            const auto& pointee = parameters().front();
            if (!is_ctype() || format == PrintFormat::EmitCangjieStrict) {
                stream << "ObjCPointer<";
                pointee.print(stream, PrintFormat::EmitCangjieStrict);
            } else {
                stream << "CPointer<";
                pointee.print(stream, format);
            }
            stream << '>';
            break;
        }
        case Kind::Function:
            if (!is_ctype() || format == PrintFormat::EmitCangjieStrict) {
                print_func_like(stream, "ObjCFunc", PrintFormat::EmitCangjieStrict);
            } else {
                print_func_like(stream, "CFunc", format);
            }
            break;
        case Kind::Block:
            print_func_like(stream, "ObjCBlock", PrintFormat::EmitCangjieStrict);
            break;
        case Kind::VArray:
            stream << Universe::get().varray().name() << '<';
            varray_element_type().print(stream, format);
            stream << ", $" << varray_size() << '>';
            break;
        case Kind::TypeParam:
            if (format == PrintFormat::Raw) {
                print_raw_type_parameter(stream, *this);
            } else {
                actual_protocol().print(stream, format);
                stream << " /*";
                print_raw_type_parameter(stream, *this);
                stream << "*/";
            }
            break;
        default:
            assert(symbol_);
            symbol_->print(stream, format);
            break;
    }
}

static void print_tricky_default_value(std::ostream& stream, std::string_view type_name)
{
    // The dirty trick is applied for printing default values of:
    // - Interface types -- instances of the interface type cannot be created.
    // - @ObjCMirror classes -- they do not have a primary constructor.
    stream << "Option<" << type_name << ">.None.getOrThrow()";
}

void Type::print_default_value(std::ostream& stream, PrintFormat format) const
{
    if (is_cj_option()) {
        stream << "None";
        return;
    }
    const auto& type_symbol = symbol();
    if (type_symbol.is<TypeParameterSymbol>()) {
        print_tricky_default_value(stream, "ObjCId");
        return;
    }
    switch (kind_) {
        case Kind::Pointer:
            print(stream, format);
            stream << (!is_ctype() || format == PrintFormat::EmitCangjieStrict ? "(CPointer<Unit>())" : "()");
            return;
        case Type::Kind::Function:
            print(stream, format);
            stream << (!is_ctype() || format == PrintFormat::EmitCangjieStrict ? "(CPointer<CFunc<() -> Unit>>())"
                                                                               : "(CPointer<Unit>())");
            return;
        case Type::Kind::Block:
            print(stream, format);
            stream << "(unsafe { ";
            print_func_like(stream, "zeroValue", format);
            stream << "() })";
            return;
        case Type::Kind::VArray:
            stream << '[';
            if (varray_size_) {
                const auto& element_type = varray_element_type();
                element_type.print_default_value(stream, format);
                for (size_t i = 1; i < varray_size_; ++i) {
                    stream << ", ";
                    element_type.print_default_value(stream, format);
                }
            }
            stream << ']';
            return;
        default:
            break;
    }
    const auto* named_type = dynamic_cast<const NamedTypeSymbol*>(&type_symbol);
    if (named_type) {
        switch (named_type->kind()) {
            case NamedTypeSymbol::Kind::Primitive:
                switch (named_type->as<PrimitiveTypeSymbol>().category()) {
                    case PrimitiveTypeCategory::Boolean:
                        stream << "false";
                        return;

                    case PrimitiveTypeCategory::SignedInteger:
                    case PrimitiveTypeCategory::UnsignedInteger:
                        stream << '0';
                        return;

                    case PrimitiveTypeCategory::FloatingPoint:
                        stream << "0.0";
                        return;

                    case PrimitiveTypeCategory::Unit:
                        stream << "()";
                        return;

                    default:
                        break;
                }
                break;
            case NamedTypeSymbol::Kind::TypeDef: {
                assert(dynamic_cast<const TypeAliasSymbol*>(named_type));
                auto canonical_type = this->canonical_type();
                const auto* named_target = dynamic_cast<const NamedTypeSymbol*>(&canonical_type.symbol());
                if (named_target) {
                    switch (named_target->kind()) {
                        case NamedTypeSymbol::Kind::Interface:
                        case NamedTypeSymbol::Kind::Protocol:
                            break;
                        default:
                            canonical_type.print_default_value(stream, format);
                            return;
                    }
                }
                break;
            }
            case NamedTypeSymbol::Kind::Unexposed:
                named_type->as<UnexposedTypeSymbol>().underlying_type().print_default_value(stream, format);
                return;
            case NamedTypeSymbol::Kind::Enum:
                Type(named_type->as<EnumDeclarationSymbol>().underlying_type()).print_default_value(stream, format);
                return;
            case NamedTypeSymbol::Kind::Interface:
            case NamedTypeSymbol::Kind::Protocol:
                print_tricky_default_value(stream, named_type->name());
                return;
            default:
                break;
        }
    }
    stream << emit_cangjie(*this) << "()";
}

Nullability Type::init_nullability(Nullability nullability) noexcept
{
    switch (kind_) {
        case Type::Kind::Named:
            switch (symbol_->as<NamedTypeSymbol>().kind()) {
                case NamedTypeSymbol::Kind::Protocol:
                case NamedTypeSymbol::Kind::Interface:
                case NamedTypeSymbol::Kind::TypeDef:
                    return nullability;
                default:
                    break;
            }
            break;
        case Type::Kind::TypeParam:
            return nullability;
        default:
            break;
    }
    return Nullability::Nonnull;
}

void Type::print_func_like(std::ostream& stream, std::string_view name, PrintFormat format) const
{
    if (parameters_.empty()) {
        stream << name << "<() -> Unit>";
    } else {
        stream << name << "<(";
        auto return_type = parameters_.begin();
        print_list(stream, std::next(return_type), parameters_.end(),
            [format](auto& stream, const auto& item) { item.print(stream, format); });
        stream << ") -> ";
        return_type->print(stream, format);
        stream << '>';
    }
}

void NamedTypeSymbol::rename(const std::string_view new_name)
{
    auto old_name = name();
    TypeLikeSymbol::rename(new_name);
    Universe::get().process_rename(*this, old_name);
}

void NamedTypeSymbol::print(std::ostream& stream, PrintFormat) const
{
    stream << escape_keyword(name());
}

void NamedTypeSymbol::set_mapping(const TypeMapping& mapping) noexcept
{
    assert(mapping_ == nullptr);
    mapping_ = &mapping;
}

TypeLikeSymbol& NamedTypeSymbol::map()
{
    if (auto* mapping = this->mapping()) {
        assert(mapping->can_map(*this));
        return mapping->map();
    }
    return *this;
}

[[nodiscard]] bool NamedTypeSymbol::is_optionable_reference() const noexcept
{
    switch (kind_) {
        case Kind::Interface:
        case Kind::Protocol:
            return true;
        default:
            return false;
    }
}

NamedTypeSymbol& EnumDeclarationSymbol::underlying_type() const noexcept
{
    assert(underlying_type_);
    return *underlying_type_;
}

EnumConstantSymbol& EnumDeclarationSymbol::add_constant(std::string name, const std::array<uint64_t, 2>& value)
{
    assert(std::all_of(
        constants_.begin(), constants_.end(), [name](const auto& constant) { return constant.name() != name; }));
    return constants_.emplace_back(std::move(name), value);
}

void EnumDeclarationSymbol::visit_impl(SymbolVisitor& visitor) const
{
    if (underlying_type_) {
        visitor.visit_type(*underlying_type_);
    }
}

[[nodiscard]] static Type underlying_unexposed_type(size_t size)
{
    auto& universe = Universe::get();
    switch (size) {
        case 0:
            return Type(universe.unit());
        case sizeof(uint8_t):
            return Type(universe.uint8());
        case sizeof(uint16_t):
            return Type(universe.uint16());
        case sizeof(uint32_t):
            return Type(universe.uint32());
        case sizeof(uint64_t):
            return Type(universe.uint64());
        default:
            return !(size % sizeof(uint64_t)) ? Type(Type(universe.uint64()), size / sizeof(uint64_t))
                : !(size % sizeof(uint32_t))  ? Type(Type(universe.uint32()), size / sizeof(uint32_t))
                : !(size % sizeof(uint16_t))  ? Type(Type(universe.uint16()), size / sizeof(uint16_t))
                                              : Type(Type(universe.uint8()), size / sizeof(uint8_t));
    }
}

UnexposedTypeSymbol::UnexposedTypeSymbol(std::string name, size_t size)
    : NamedTypeSymbol(Kind::Unexposed, std::move(name)), underlying_type_(underlying_unexposed_type(size))
{
}

void UnexposedTypeSymbol::print(std::ostream& stream, PrintFormat format) const
{
    underlying_type().print(stream, format);
    stream << " /*" << name() << "*/";
}

[[nodiscard]] static bool is_ctype_by_default(NamedTypeSymbol::Kind kind, std::string_view name) noexcept
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

TypeDeclarationSymbol::TypeDeclarationSymbol(const Kind kind, std::string name) noexcept
    : NamedTypeSymbol(kind, std::move(name)),
      is_ctype_(is_ctype_by_default(kind, this->name())),
      contains_pointer_or_func_(false),
      static_instance_clashes_resolved_(false),
      override_returns_resolved_(false)
{
}

void TypeDeclarationSymbol::add_base(TypeDeclarationSymbol& base)
{
    bases_.push_back(&base);
}

const TypeParameterSymbol& TypeDeclarationSymbol::parameter(size_t index) const noexcept
{
    return parameters_[index];
}

TypeParameterSymbol& TypeDeclarationSymbol::parameter(size_t index) noexcept
{
    return parameters_[index];
}

void TypeDeclarationSymbol::add_parameter(std::string name)
{
    assert(std::all_of(
        parameters_.begin(), parameters_.end(), [&name](const auto& parameter) { return parameter.name() != name; }));

    parameters_.emplace_back(std::move(name));
}

void TypeDeclarationSymbol::member_remove(size_t index)
{
    auto it = members_.begin();
    std::advance(it, index);
    switch (kind()) {
        case Kind::Struct:
        case Kind::Union: {
            assert(it->kind() == NonTypeSymbol::Kind::Field);
            auto removing_ctype = it->return_type().is_ctype();
            members_.erase(it);
            if (!removing_ctype) {
                is_ctype_ = all_of_members([](const auto& member) { return member.return_type().is_ctype(); });
            }
            contains_pointer_or_func_ =
                any_of_members([](const auto& member) { return member.return_type().contains_pointer_or_func(); });
            break;
        }
        default:
            members_.erase(it);
            break;
    }
}

NonTypeSymbol& TypeDeclarationSymbol::add_member_method(std::string name, Type return_type, Modifiers modifiers)
{
    // No clash detection, otherwise might assert on method overloads

    assert(kind() == Kind::Interface || kind() == Kind::Protocol || kind() == Kind::TopLevel);
    return members_.emplace_back(std::move(name), NonTypeSymbol::Kind::MemberMethod, std::move(return_type), modifiers);
}

NonTypeSymbol& TypeDeclarationSymbol::add_constructor(std::string name, Type return_type)
{
    assert(kind() == Kind::Interface || kind() == Kind::Protocol);
    return members_.emplace_back(std::move(name), NonTypeSymbol::Kind::Constructor, std::move(return_type));
}

NonTypeSymbol& TypeDeclarationSymbol::add_field(std::string name, Type type, Modifiers modifiers)
{
    assert(kind() == Kind::Struct || kind() == Kind::Union);

    // Only bit-fields can be unnamed
    assert(!name.empty() || (modifiers & ModifierBitField));

    // And once it is named, no other field with this name should exist
    assert(name.empty() ||
        all_of_members([&name](const auto& member) { return !member.is_field() || member.name() != name; }));

    auto& member = members_.emplace_back(std::move(name), NonTypeSymbol::Kind::Field, std::move(type), modifiers);
    if (is_ctype_ && !member.return_type().is_ctype()) {
        is_ctype_ = false;
    }
    if (member.return_type().contains_pointer_or_func()) {
        contains_pointer_or_func_ = true;
    }
    return member;
}

NonTypeSymbol& TypeDeclarationSymbol::add_instance_variable(std::string name, Type type, Modifiers modifiers)
{
    assert(kind() == Kind::Interface);
    assert(all_of_members([&name](const auto& member) { return member.name() != name; }));

    auto& ivar =
        members_.emplace_back(std::move(name), NonTypeSymbol::Kind::InstanceVariable, std::move(type), modifiers);
    if (is_ctype_ && !ivar.return_type().is_ctype()) {
        is_ctype_ = false;
    }
    if (ivar.return_type().contains_pointer_or_func()) {
        contains_pointer_or_func_ = true;
    }
    return ivar;
}

NonTypeSymbol& TypeDeclarationSymbol::add_property(
    std::string name, std::string getter, std::string setter, Modifiers modifiers)
{
    assert(kind() == Kind::Interface || kind() == Kind::Protocol);

    return members_.emplace_back(std::move(name), std::move(getter), std::move(setter), modifiers);
}

void TypeDeclarationSymbol::mark_override_return_clashes_resolved() noexcept
{
    assert(!override_returns_resolved_);
    override_returns_resolved_ = true;
}

void TypeDeclarationSymbol::mark_static_instance_clashes_resolved() noexcept
{
    assert(!static_instance_clashes_resolved_);
    static_instance_clashes_resolved_ = true;
}

void TypeDeclarationSymbol::visit_impl(SymbolVisitor& visitor) const
{
    // It could make sense to analyze if infinite recursion is possible her.  With
    // CRTP for example.
    for (auto& base : this->bases()) {
        visitor.visit_type(base);
    }
    for (auto& member : this->members()) {
        visitor.visit_member(member);
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

TypeAliasSymbol::TypeAliasSymbol(std::string name) noexcept : NamedTypeSymbol(Kind::TypeDef, std::move(name))
{
}

void TypeAliasSymbol::print(std::ostream& stream, PrintFormat format) const
{
    const auto& target = this->target();
    if (target.has_symbol_assigned() && name() == target.name()) {
        // typedef struct S S;
        target.print(stream, format);
        return;
    }
    if (mode != Mode::EXPERIMENTAL && format == PrintFormat::EmitCangjieStrict) {
        auto canonical_type = this->canonical_type();
        if (canonical_type.is_ctype() && canonical_type.contains_pointer_or_func()) {
            stream << emit_cangjie_strict(canonical_type) << " /*" << emit_cangjie(*this) << "*/";
            return;
        }
    }
    NamedTypeSymbol::print(stream, format);
}

void TypeAliasSymbol::visit_impl(SymbolVisitor& visitor) const
{
    const auto& target = this->target();
    if (target.has_symbol_assigned()) {
        visitor.visit_type(target);
    }
}

void NonTypeSymbol::rename(std::string_view new_name)
{
    assert(!new_name.empty());
    if (selector_attribute_.empty()) {
        selector_attribute_ = name();
    }
    FileLevelSymbol::rename(new_name);
}

bool NonTypeSymbol::is_ctype() const noexcept
{
    return std::all_of(parameters_.begin(), parameters_.end(), [](const auto& p) { return p.type().is_ctype(); }) &&
        return_type_.is_ctype();
}

void NonTypeSymbol::visit_impl(SymbolVisitor& visitor) const
{
    for (auto& parameter : this->parameters()) {
        visitor.visit_type(parameter.type());
    }

    if (kind_ != Kind::Property) {
        visitor.visit_type(return_type());
    }
}

const Type& NonTypeSymbol::return_type() const noexcept
{
    // Property does not store its return type here.  Look for return type of the
    // getter.
    assert(kind_ != Kind::Property);

    return return_type_;
}

Type& NonTypeSymbol::return_type() noexcept
{
    // Property does not store its return type here.  Look for return type of the
    // getter.
    assert(kind_ != Kind::Property);

    return return_type_;
}

void NonTypeSymbol::set_return_type(Type return_type) noexcept
{
    assert(kind_ != Kind::Property);
    return_type_ = std::move(return_type);
}

void NonTypeSymbol::add_parameter(std::string name, Type type)
{
    assert(is_method());
    parameters_.emplace_back(std::move(name), std::move(type));
}

} // namespace objcgen
