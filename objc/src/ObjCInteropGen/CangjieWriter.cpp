// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "CangjieWriter.h"

#include <fstream>
#include <iostream>
#include <set>
#include <sstream>

#include "Logging.h"
#include "Mode.h"
#include "Package.h"
#include "SingleDeclarationSymbolVisitor.h"
#include "Strings.h"
#include "Universe.h"

namespace objcgen {

static constexpr char INDENT[] = "    ";
static constexpr std::size_t INDENT_LENGTH = sizeof(INDENT) - 1;

static_assert(INDENT_LENGTH == 4);

constexpr char COMMENT[] = "// ";
constexpr auto COMMENT_LENGTH = sizeof(COMMENT) - 1;

class IndentingStringBuf final : public std::streambuf {
public:
    IndentingStringBuf() : buf_(std::ios_base::out)
    {
    }

    void indent() noexcept
    {
        indentation_ += INDENT;
    }

    void dedent() noexcept
    {
        assert(ends_with(indentation_, INDENT));
        indentation_.resize(indentation_.size() - INDENT_LENGTH);
    }

    void set_comment() noexcept
    {
        indentation_ += COMMENT;
    }

    void reset_comment() noexcept
    {
        assert(ends_with(indentation_, COMMENT));
        indentation_.resize(indentation_.size() - COMMENT_LENGTH);
    }

    std::string str() const
    {
        return buf_.str();
    }

protected:
    int_type overflow(const int_type ch) override
    {
        if (start_line_) {
            // Print `//` (with proper indentation) even for empty lines
            auto count = static_cast<std::streamsize>(indentation_.length());
            if (buf_.sputn(indentation_.data(), count) != count) {
                return traits_type::eof();
            }
        }
        start_line_ = ch == '\n';
        return buf_.sputc(traits_type::to_char_type(ch));
    }

private:
    std::stringbuf buf_;
    /**
     * Indentation spaces printed currently at the beginning of each line.
     * Includes `//` comments, if any.
     */
    std::string indentation_;
    bool start_line_ = true;
};

class IndentingStringStream : public std::ostream {
public:
    IndentingStringStream() : std::ostream(&fos_buf)
    {
    }

    std::string str() const
    {
        return fos_buf.str();
    }

    void indent() noexcept
    {
        fos_buf.indent();
    }

    void dedent() noexcept
    {
        fos_buf.dedent();
    }

    void set_comment() noexcept
    {
        fos_buf.set_comment();
    }

    void reset_comment() noexcept
    {
        fos_buf.reset_comment();
    }

private:
    IndentingStringBuf fos_buf;
};

static std::string_view current_package_name;
static std::set<std::string> imports;

class PackageFileScope final {
    const std::string_view package_name_;

public:
    [[nodiscard]] explicit PackageFileScope(std::string_view package_name) noexcept : package_name_(package_name)
    {
        assert(current_package_name.empty());
        assert(!package_name.empty());
        assert(imports.empty());
        current_package_name = package_name;
    }

    ~PackageFileScope()
    {
        assert(current_package_name == package_name_);
        current_package_name = {};
        imports.clear();
    }

    PackageFileScope(const PackageFileScope& other) = delete;

    PackageFileScope(PackageFileScope&& other) noexcept = delete;

    PackageFileScope& operator=(const PackageFileScope& other) = delete;

    PackageFileScope& operator=(PackageFileScope&& other) noexcept = delete;
};

static std::string symbol_to_import_name(const FileLevelSymbol& symbol)
{
    assert(!current_package_name.empty());
    const auto& symbol_package_name = symbol.cangjie_package_name();
    if (!symbol_package_name.empty() && symbol_package_name != current_package_name) {
        auto import_name = symbol_package_name;
        import_name += ".";
        import_name += symbol.name();
        return import_name;
    }
    return {};
}

class ImportCollectVisitor final : public SingleDeclarationSymbolVisitor {
public:
    [[nodiscard]] explicit ImportCollectVisitor() : SingleDeclarationSymbolVisitor(false)
    {
    }

    ImportCollectVisitor(const ImportCollectVisitor& other) = delete;

    ImportCollectVisitor(ImportCollectVisitor&& other) noexcept = delete;

    ImportCollectVisitor& operator=(const ImportCollectVisitor& other) = delete;

    ImportCollectVisitor& operator=(ImportCollectVisitor&& other) noexcept = delete;

protected:
    void visit_impl(FileLevelSymbol* owner, FileLevelSymbol* value, SymbolProperty symbol_property, bool) override
    {
        assert(value);

        // If this is a type argument of an Objective-C generic type, ignore it.  Type
        // arguments are erased and may be printed inside comments only.
        if (dynamic_cast<NamedTypeSymbol*>(owner) && symbol_property == SymbolProperty::TypeArgument) {
            return;
        }

        const auto* constructed_type = dynamic_cast<const ConstructedTypeSymbol*>(value);
        if (constructed_type) {
            value = constructed_type->original();
            assert(value);
        }
        if (auto import_name = symbol_to_import_name(*value); !import_name.empty()) {
            imports.emplace(std::move(import_name));
        }
    }
};

static void collect_import(TypeLikeSymbol& symbol)
{
    ImportCollectVisitor().visit(&symbol);
}

// Currently in the NORMAL mode, Objective-C compatible types are primitives,
// @C structures, ObjCPointer, ObjCFunc, ObjCBlock, and classes/interfaces.
// But not CPointer, CFunc, or VArray.
static bool is_objc_compatible(const TypeLikeSymbol& type)
{
    assert(normal_mode());
    if (&type == &Universe::get().sel()) {
        return false;
    }
    if (dynamic_cast<const TypeParameterSymbol*>(&type)) {
        // Type parameters are printed as ObjCId, which is Objective-C compatible
        return true;
    }
    const auto* ptr = dynamic_cast<const PointerTypeSymbol*>(&type);
    if (ptr) {
        return is_objc_compatible(ptr->pointee());
    }
    const auto* func = dynamic_cast<const FuncLikeTypeSymbol*>(&type);
    if (func) {
        const auto& parameters = *func->parameters();
        auto n = parameters.item_count();
        for (size_t i = 0; i < n; ++i) {
            if (!is_objc_compatible(*parameters.item(i))) {
                return false;
            }
        }
        auto* return_type = func->return_type();
        return !return_type || is_objc_compatible(*return_type);
    }
    const auto* named = dynamic_cast<const NamedTypeSymbol*>(&type);
    if (!named) {
        return false;
    }
    switch (named->kind()) {
        case NamedTypeSymbol::Kind::Struct:
        case NamedTypeSymbol::Kind::Union:
            return type.is_ctype();
        case NamedTypeSymbol::Kind::Interface:
            return type.name() != "Protocol";
        case NamedTypeSymbol::Kind::Primitive:
        case NamedTypeSymbol::Kind::Protocol:
        case NamedTypeSymbol::Kind::Enum:
            return true;
        case NamedTypeSymbol::Kind::TypeDef:
            return is_objc_compatible(type.canonical_type());
        default:
            return false;
    }
}

static bool write_type_alias(IndentingStringStream& output, TypeAliasSymbol& alias)
{
    auto* target = alias.target();
    assert(target);
    if (alias.name() == target->name()) {
        // typedef struct S S;
        // In the Cangjie output, the target symbol is used directly instead of this typedef, do not print it.
        return false;
    }

    auto supported = !normal_mode() || alias.is_ctype() || is_objc_compatible(alias);
    if (supported) {
        collect_import(*target);
    } else {
        output.set_comment();
    }
    output << "public type " << emit_cangjie(alias) << " = " << emit_cangjie(*target) << '\n';
    if (!supported) {
        output.reset_comment();
    }
    return true;
}

class DefaultValuePrinter;
static std::ostream& operator<<(std::ostream& stream, const DefaultValuePrinter& op);

class DefaultValuePrinter {
public:
    explicit DefaultValuePrinter(const NonTypeSymbol& symbol, SymbolPrinter type_printer) noexcept
        : symbol_(symbol), type_printer_(type_printer)
    {
    }

    friend std::ostream& operator<<(std::ostream& stream, const DefaultValuePrinter& op);

private:
    const NonTypeSymbol& symbol_;
    const SymbolPrinter type_printer_;
};

static DefaultValuePrinter default_value(
    const NonTypeSymbol& symbol, const TypeLikeSymbol& type, SymbolPrintFormat format)
{
    return DefaultValuePrinter(symbol, SymbolPrinter(type, format));
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

static std::ostream& operator<<(std::ostream& stream, const DefaultValuePrinter& op)
{
    const auto& type = op.type_printer_.symbol();
    if (dynamic_cast<const TypeParameterSymbol*>(&type)) {
        print_tricky_default_value(stream, "ObjCId", op.symbol_.is_nullable());
        return stream;
    }
    const auto* ptr = dynamic_cast<const PointerTypeSymbol*>(&type);
    if (ptr) {
        return stream << op.type_printer_
                      << (!ptr->is_ctype() || op.type_printer_.format() == SymbolPrintFormat::EmitCangjieStrict
                                 ? "(CPointer<Unit>())"
                                 : "()");
    }
    const auto* func = dynamic_cast<const FuncTypeSymbol*>(&type);
    if (func) {
        return stream << op.type_printer_
                      << (!func->is_ctype() || op.type_printer_.format() == SymbolPrintFormat::EmitCangjieStrict
                                 ? "(CPointer<CFunc<() -> Unit>>())"
                                 : "(CPointer<Unit>())");
    }
    if (dynamic_cast<const BlockTypeSymbol*>(&type)) {
        return stream << op.type_printer_ << "(CPointer<NativeBlockABI>())";
    }
    const auto* named_type = dynamic_cast<const NamedTypeSymbol*>(&type);
    if (named_type) {
        switch (named_type->kind()) {
            case NamedTypeSymbol::Kind::Primitive: {
                assert(dynamic_cast<const PrimitiveTypeSymbol*>(named_type));
                const auto& primitive_type = static_cast<const PrimitiveTypeSymbol&>(*named_type);
                switch (primitive_type.category()) {
                    case PrimitiveTypeCategory::Boolean:
                        return stream << "false";

                    case PrimitiveTypeCategory::SignedInteger:
                    case PrimitiveTypeCategory::UnsignedInteger:
                        return stream << '0';

                    case PrimitiveTypeCategory::FloatingPoint:
                        return stream << "0.0";

                    case PrimitiveTypeCategory::Unit:
                        return stream << "()";

                    default:
                        break;
                }
                break;
            }
            case NamedTypeSymbol::Kind::TypeDef: {
                assert(dynamic_cast<const TypeAliasSymbol*>(named_type));
                const auto& target = named_type->canonical_type();
                const auto* named_target = dynamic_cast<const NamedTypeSymbol*>(&target);
                if (named_target && named_target->is(NamedTypeSymbol::Kind::Primitive)) {
                    assert(dynamic_cast<const PrimitiveTypeSymbol*>(named_target));
                    const auto& primitive_target = static_cast<const PrimitiveTypeSymbol&>(*named_target);
                    switch (primitive_target.category()) {
                        case PrimitiveTypeCategory::SignedInteger:
                        case PrimitiveTypeCategory::UnsignedInteger:
                            return stream << "unsafe{zeroValue<" << named_type->name() << ">()}";
                        default:
                            break;
                    }
                }
                return stream << default_value(op.symbol_, target, op.type_printer_.format());
            }
            case NamedTypeSymbol::Kind::Unexposed:
                assert(dynamic_cast<const UnexposedTypeSymbol*>(named_type));
                return stream << default_value(op.symbol_, named_type->canonical_type(), op.type_printer_.format());
            case NamedTypeSymbol::Kind::Enum:
                assert(dynamic_cast<const EnumDeclarationSymbol*>(named_type));
                return stream << default_value(op.symbol_,
                           static_cast<const EnumDeclarationSymbol&>(*named_type).underlying_type(),
                           op.type_printer_.format());
            case NamedTypeSymbol::Kind::Interface:
            case NamedTypeSymbol::Kind::Protocol:
                print_tricky_default_value(stream, named_type->name(), op.symbol_.is_nullable());
                return stream;
            default:
                break;
        }
    } else {
        const auto* varray = dynamic_cast<const VArraySymbol*>(&type);
        if (varray) {
            stream << '[';
            if (varray->size_) {
                auto value = default_value(op.symbol_, *varray->element_type_, op.type_printer_.format());
                stream << value;
                for (size_t i = 1; i < varray->size_; ++i) {
                    stream << ", " << value;
                }
            }
            return stream << ']';
        }
    }
    return stream << emit_cangjie(type) << "()";
}

static void print_enum_constant_value(
    std::ostream& output, const NamedTypeSymbol& underlying_type, const EnumConstantSymbol& constant)
{
    const auto& canonical_type = underlying_type.canonical_type();
    const auto* primitive_type = dynamic_cast<const PrimitiveTypeSymbol*>(&canonical_type);
    if (primitive_type) {
        // Avoid the "number exceeds the value range of type" Cangjie compiler error by
        // printing the value in a type-specific way.
        switch (primitive_type->category()) {
            case PrimitiveTypeCategory::SignedInteger:
                switch (primitive_type->size()) {
                    case PrimitiveSize::One:
                        output << static_cast<int>(constant.value<int8_t>());
                        return;
                    case PrimitiveSize::Two:
                        output << constant.value<int16_t>();
                        return;
                    case PrimitiveSize::Four:
                        output << constant.value<int32_t>();
                        return;
                    case PrimitiveSize::Eight:
                        output << constant.value<int64_t>();
                        return;
                    default:
                        break;
                }
                break;
            case PrimitiveTypeCategory::UnsignedInteger:
                switch (primitive_type->size()) {
                    case PrimitiveSize::One:
                        output << static_cast<uint32_t>(constant.value<uint8_t>());
                        return;
                    case PrimitiveSize::Two:
                        output << constant.value<uint16_t>();
                        return;
                    case PrimitiveSize::Four:
                        output << constant.value<uint32_t>();
                        return;
                    // PrimitiveSize::Eight is handled properly by fallback case.
                    default:
                        break;
                }
                break;
            default:
                break;
        }
    } else {
        auto& universe = Universe::get();
        if (&canonical_type == &universe.int128()) {
            output << "ObjCInt128(" << constant.value128_lo<int64_t>() << ", " << constant.value128_hi<int64_t>()
                   << ')';
            return;
        }
        if (&canonical_type == &universe.uint128()) {
            output << "ObjCUInt128(" << constant.value128_lo<uint64_t>() << ", " << constant.value128_hi<uint64_t>()
                   << ')';
            return;
        }
    }
    output << constant.value<uint64_t>();
}

static bool is_objc_compatible_parameters(NonTypeSymbol& method) noexcept
{
    for (const auto& parameter : method.parameters()) {
        if (!is_objc_compatible(*parameter.type())) {
            return false;
        }
    }
    return true;
}

template <class Symbol>
void write_type(std::ostream& output, const Symbol& symbol, TypeLikeSymbol& type, SymbolPrintFormat format)
{
    output << ": ";
    if (symbol.is_nullable()) {
        output << '?';
    }
    output << SymbolPrinter(type, format);
}

static void write_method_parameters(std::ostream& output, const NonTypeSymbol& method, SymbolPrintFormat format)
{
    output << '(';
    auto parameter_count = method.parameter_count();
    for (std::size_t j = 0; j < parameter_count; ++j) {
        if (j != 0) {
            output << ", ";
        }

        auto& parameter_symbol = method.parameter(j);
        auto* parameter_type = parameter_symbol.type();
        assert(parameter_type);
        output << escape_keyword(parameter_symbol.name());
        write_type(output, parameter_symbol, *parameter_type, format);
        collect_import(*parameter_type);
    }
    output << ')';
}

static void write_foreign_name(
    std::ostream& output, std::string_view attribute, std::string_view value, bool hide_foreign_name)
{
    // FE supports foreign name attributes in @ObjCMirror classes only.  In the
    // GENERATE_DEFINITIONS mode, where @ObjCMirror is not used, the foreign name
    // attributes are commented out.
    if (hide_foreign_name) {
        output << "/* ";
    }
    output << attribute << "[\"" << value << "\"]";
    if (hide_foreign_name) {
        output << " */";
    }
    output << ' ';
}

static void write_foreign_name(std::ostream& output, const NonTypeSymbol& method)
{
    // The foreign name attributes could not appear on overridden declaration
    if (method.is_override()) {
        return;
    }

    constexpr std::string_view attribute = "@ForeignName";
    const auto& selector_attribute = method.selector_attribute();
    if (!selector_attribute.empty()) {
        write_foreign_name(output, attribute, selector_attribute, generate_definitions_mode());
    } else if (method.is_constructor() && method.name() != "init") {
        write_foreign_name(output, attribute, method.name(), generate_definitions_mode());
    }
}

static bool same_types(const TypeLikeSymbol* type1, const TypeLikeSymbol* type2)
{
    const auto* alias = dynamic_cast<const TypeAliasSymbol*>(type1);
    if (alias) {
        type1 = &alias->canonical_type();
    }
    alias = dynamic_cast<const TypeAliasSymbol*>(type2);
    if (alias) {
        type2 = &alias->canonical_type();
    }

    const auto* constructed1 = dynamic_cast<const ConstructedTypeSymbol*>(type1);
    if (constructed1) {
        const auto* constructed2 = dynamic_cast<const ConstructedTypeSymbol*>(type2);
        return constructed2 && same_types(constructed1->original(), constructed2->original());
    }

    const auto* pointer1 = dynamic_cast<const PointerTypeSymbol*>(type1);
    if (pointer1) {
        auto* pointer2 = dynamic_cast<const PointerTypeSymbol*>(type2);
        return pointer2 && same_types(&pointer1->pointee(), &pointer2->pointee());
    }

    const auto* func1 = dynamic_cast<const FuncLikeTypeSymbol*>(type1);
    if (func1) {
        auto* func2 = dynamic_cast<const FuncTypeSymbol*>(type2);
        if (!func2 || !same_types(func1->return_type(), func2->return_type())) {
            return false;
        }
        const auto& parameters1 = *func1->parameters();
        auto n1 = parameters1.item_count();
        const auto& parameters2 = *func2->parameters();
        auto n2 = parameters2.item_count();
        if (n1 != n2) {
            return false;
        }
        for (size_t i = 0; i < n1; ++i) {
            if (!same_types(parameters1.item(i), parameters2.item(i))) {
                return false;
            }
        }
        return true;
    }

    const auto* varray1 = dynamic_cast<const VArraySymbol*>(type1);
    if (varray1) {
        const auto* varray2 = dynamic_cast<const VArraySymbol*>(type2);
        return varray2 && varray1->size() == varray2->size() &&
            same_types(&varray1->element_type(), &varray2->element_type());
    }

    return type1 == type2;
}

static bool is_overloading_constructor(TypeDeclarationSymbol& type, const NonTypeSymbol& constructor)
{
    assert(constructor.is_constructor());
    auto parameter_count = constructor.parameter_count();
    for (const auto& member : type.members()) {
        if (&member == &constructor || !member.is_constructor() || member.parameter_count() != parameter_count) {
            continue;
        }
        auto overloading = true;
        for (size_t i = 0; i < parameter_count; ++i) {
            if (!same_types(member.parameter(i).type(), constructor.parameter(i).type())) {
                overloading = false;
                break;
            }
        }
        if (overloading) {
            return true;
        }
    }
    return false;
}

static NonTypeSymbol* get_overridden_method(TypeDeclarationSymbol& decl, const NonTypeSymbol& prop)
{
    const auto& selector = prop.getter();
    auto is_static = prop.is_static();
    for (auto& base_decl : decl.bases()) {
        for (auto& member : base_decl.members()) {
            if (member.is_member_method() && member.is_static() == is_static && member.selector() == selector) {
                return &member;
            }
        }
        auto* overridden_method = get_overridden_method(base_decl, prop);
        if (overridden_method) {
            return overridden_method;
        }
    }
    return nullptr;
}

static NonTypeSymbol* get_property(TypeDeclarationSymbol& decl, const NonTypeSymbol& getter_or_setter)
{
    const auto& selector = getter_or_setter.selector();
    auto is_static = getter_or_setter.is_static();
    for (auto& member : decl.members()) {
        if (member.is_property() && member.is_static() == is_static &&
            (member.getter() == selector || member.setter() == selector)) {
            return &member;
        }
    }
    return nullptr;
}

static NonTypeSymbol* get_method_by_selector(TypeDeclarationSymbol& decl, const std::string& selector, bool is_static)
{
    for (auto& member : decl.members()) {
        if (member.is_member_method() && member.is_static() == is_static && member.selector() == selector) {
            return &member;
        }
    }
    return nullptr;
}

static NonTypeSymbol* get_overridden_property(TypeDeclarationSymbol& decl, const std::string& getter, bool is_static)
{
    for (auto& base_decl : decl.bases()) {
        for (auto& member : base_decl.members()) {
            if (member.is_property() && member.is_static() == is_static && member.getter() == getter) {
                return &member;
            }
        }
        auto* overridden_prop = get_overridden_property(base_decl, getter, is_static);
        if (overridden_prop) {
            return overridden_prop;
        }
    }
    return nullptr;
}

static void print_optional(std::ostream& output, const NonTypeSymbol& member)
{
    if (member.is_optional()) {
        if (member.is_property() && normal_mode()) {
            return;
        }
        if (generate_definitions_mode()) {
            output << "// ";
        }
        output << "@ObjCOptional\n";
    }
}

static bool is_standard_setter_name(std::string_view prop_name, std::string_view setter_name)
{
    assert(!prop_name.empty());
    constexpr char standard_setter_prefix[] = "set";
    constexpr auto standard_setter_prefix_size = std::size(standard_setter_prefix) - 1;
    return setter_name.size() > standard_setter_prefix_size + 1 && setter_name.back() == ':' &&
        starts_with(setter_name, standard_setter_prefix) &&
        setter_name[standard_setter_prefix_size] == static_cast<char>(toupper(prop_name.front())) &&
        setter_name.substr(standard_setter_prefix_size + 1, prop_name.size() - 1) == prop_name.substr(1);
}

static void print_getter_setter_names(std::ostream& output, const NonTypeSymbol& prop)
{
    assert(prop.kind() == NonTypeSymbol::Kind::Property);
    const auto& name = prop.name();
    const auto& getter_name = prop.getter();
    bool standard_getter = getter_name == name;
    auto hide_foreign_name = generate_definitions_mode();
    if (prop.is_readonly()) {
        if (!standard_getter) {
            write_foreign_name(output, "@ForeignGetterName", getter_name, hide_foreign_name);
        }
    } else {
        const auto& setter_name = prop.setter();
        if (is_standard_setter_name(name, setter_name)) {
            if (!standard_getter) {
                write_foreign_name(output, "@ForeignGetterName", getter_name, hide_foreign_name);
            }
        } else if (standard_getter) {
            write_foreign_name(output, "@ForeignSetterName", setter_name, hide_foreign_name);
        } else if (is_standard_setter_name(getter_name, setter_name)) {
            write_foreign_name(output, "@ForeignName", getter_name, hide_foreign_name);
        } else {
            write_foreign_name(output, "@ForeignGetterName", getter_name, hide_foreign_name);
            write_foreign_name(output, "@ForeignSetterName", setter_name, hide_foreign_name);
        }
    }
}

enum class FuncKind { TopLevelFunc, InterfaceMethod, ClassMethod };

static void write_function(
    IndentingStringStream& output, FuncKind kind, NonTypeSymbol& function, SymbolPrintFormat format)
{
    auto* return_type = function.return_type();
    assert(return_type);
    auto supported = !normal_mode() || (is_objc_compatible(*return_type) && is_objc_compatible_parameters(function));
    if (!supported) {
        output.set_comment();
    }
    bool is_ctype;
    switch (kind) {
        case FuncKind::TopLevelFunc: {
            is_ctype = function.is_ctype();

            // Foreign @C functions cannot have a foreign name
            const auto& selector_attribute = function.selector_attribute();
            if (is_ctype) {
                output << "foreign ";
                if (!selector_attribute.empty()) {
                    write_foreign_name(output, "@ForeignName", selector_attribute, true);
                }
            } else {
                auto generate_definitions = generate_definitions_mode();
                if (!generate_definitions) {
                    output << "@ObjCMirror\n";
                    format = SymbolPrintFormat::EmitCangjieStrict;
                }
                if (!selector_attribute.empty()) {
                    write_foreign_name(output, "@ForeignName", selector_attribute, generate_definitions);
                }
                output << "public ";
            }
            break;
        }
        case FuncKind::InterfaceMethod:
            is_ctype = false;
            print_optional(output, function);
            write_foreign_name(output, function);
            break;
        default:
            assert(kind == FuncKind::ClassMethod);
            is_ctype = false;
            write_foreign_name(output, function);
            output << "public ";
            break;
    }
    if (function.is_static()) {
        // In Objective-C, the overridden static method can have different parameter
        // types (co/contra-variant pointers).  In Cangjie, the types must strictly
        // match.  Consider printing "redef" at least when it is allowed in Cangjie.
        if constexpr ((false)) {
            if (function.is_override()) {
                output << "redef ";
            }
        }
        output << "static ";
    } else {
        if (kind == FuncKind::ClassMethod) {
            output << "open ";
        }
        // In Objective-C, the overridden method can have different parameter types
        // (co/contra-variant pointers).  In Cangjie, the types must strictly coincide.
        // Consider printing "override" at least when it is allowed in Cangjie.
        if constexpr ((false)) {
            if (function.is_override()) {
                output << "override ";
            }
        }
    }
    output << "func " << escape_keyword(function.name());
    write_method_parameters(output, function, format);
    write_type(output, function, *return_type, format);
    if (generate_definitions_mode() && !is_ctype) {
        if (return_type->is_unit()) {
            output << " { }";
        } else {
            output << " { " << default_value(function, *return_type, format) << " }";
        }
    }
    if (supported) {
        collect_import(*return_type);
    } else {
        output.reset_comment();
    }
    output << '\n';
}

enum class DeclKind { CStruct, ObjCStruct, Interface, Class };

// Whether a property or field with the specified type and name is currently
// supported by FE in declarations of the specified kind.  If not, then in the
// NORMAL mode it will be commented out. In the EXPERIMENTAL and
// GENERATE_DEFINITIONS modes, any property/field is supported.
static bool is_field_type_supported(DeclKind decl_kind, TypeLikeSymbol& type, const std::string& name)
{
    if (!normal_mode()) {
        return true;
    }
    switch (decl_kind) {
        case DeclKind::CStruct:
            assert(type.is_ctype());
            return true;
        case DeclKind::ObjCStruct:
            return true;
        default:
            assert(decl_kind == DeclKind::Class || decl_kind == DeclKind::Interface);

            // Current FE fails to process a field or property of an @ObjCMirror class if
            // the field and its type have the same name (no such problem in non-@ObjCMirror
            // declarations).  As a workaround, comment out such fields.
            return name != type.name() && is_objc_compatible(type);
    }
}

static void print_objcmirror_attribute(std::ostream& output, bool supported)
{
    auto hide_objcmirror_attribute = !supported;
    if (hide_objcmirror_attribute) {
        output << "/* ";
    }
    output << "@ObjCMirror";
    if (hide_objcmirror_attribute) {
        output << " */";
    }
    output << '\n';
}

class TypeDeclarationWriter {
public:
    TypeDeclarationWriter(IndentingStringStream& output, TypeDeclarationSymbol& type) noexcept;

    void write();

private:
    void write_property(const NonTypeSymbol& prop);
    void write_constructor(NonTypeSymbol& constructor);
    void write_instance_variable(const NonTypeSymbol& ivar);
    void write_field(const NonTypeSymbol& field);

    IndentingStringStream& output_;
    TypeDeclarationSymbol& type_;
    DeclKind decl_kind_;
    SymbolPrintFormat format_;
    bool any_constructor_exists_;
    bool default_constructor_exists_;
};

TypeDeclarationWriter::TypeDeclarationWriter(IndentingStringStream& output, TypeDeclarationSymbol& type) noexcept
    : output_(output), type_(type), any_constructor_exists_(false), default_constructor_exists_(false)
{
}

void TypeDeclarationWriter::write_property(const NonTypeSymbol& prop)
{
    assert(prop.is_property());
    auto is_static = prop.is_static();
    const auto& getter_name = prop.getter();
    auto* getter = get_method_by_selector(type_, getter_name, is_static);
    assert(getter);
    if (!get_overridden_method(type_, prop) && !get_overridden_property(type_, getter_name, is_static)) {
        auto* return_type = getter->return_type();
        assert(return_type);
        assert(!return_type->is_unit());
        const auto& name = prop.name();
        auto supported = is_field_type_supported(decl_kind_, *return_type, name);
        if (!supported) {
            output_.set_comment();
        }

        // Only interfaces can have @ObjCOptional members, not classes
        assert(!prop.is_optional() || decl_kind_ == DeclKind::Interface);
        if (decl_kind_ == DeclKind::Interface) {
            print_optional(output_, prop);
        }

        print_getter_setter_names(output_, prop);
        if (decl_kind_ != DeclKind::Interface) {
            output_ << "public ";
        }
        if (is_static) {
            output_ << "static ";
        } else if (decl_kind_ != DeclKind::Interface) {
            output_ << "open ";
        }
        if (!prop.is_readonly()) {
            output_ << "mut ";
        }
        output_ << "prop " << escape_keyword(name);
        write_type(output_, *getter, *return_type, format_);
        if (generate_definitions_mode()) {
            output_ << " {\n";
            output_.indent();
            output_ << "get() { " << default_value(*getter, *return_type, format_) << " }\n";
            if (!prop.is_readonly()) {
                output_ << "set(v) { }\n";
            }
            output_.dedent();
            output_ << '}';
        }
        if (supported) {
            collect_import(*return_type);
        } else {
            output_.reset_comment();
        }
        output_ << '\n';
    }
}

void TypeDeclarationWriter::write_constructor(NonTypeSymbol& constructor)
{
    assert(constructor.is_constructor());
    auto supported =
        decl_kind_ != DeclKind::Interface && (!normal_mode() || is_objc_compatible_parameters(constructor));
    if (supported) {
        any_constructor_exists_ = true;
        if (!default_constructor_exists_) {
            default_constructor_exists_ = constructor.parameter_count() == 0;
        }
    } else {
        output_.set_comment();
    }
    if (is_overloading_constructor(type_, constructor)) {
        if (!generate_definitions_mode()) {
            output_ << "@ObjCInit ";
        }
        write_foreign_name(output_, constructor);
        if (decl_kind_ != DeclKind::Interface) {
            output_ << "public ";
        }
        output_ << "static func " << escape_keyword(constructor.name());
        write_method_parameters(output_, constructor, format_);

        // FE requires the return type to be strictly the declaring class
        auto* return_type = normal_mode() ? &type_ : constructor.return_type();
        assert(return_type);

        write_type(output_, constructor, *return_type, format_);
        if (generate_definitions_mode() && decl_kind_ != DeclKind::Interface) {
            output_ << " { " << default_value(constructor, *return_type, format_) << " }";
        }
        if (supported) {
            collect_import(*return_type);
        }
    } else {
        write_foreign_name(output_, constructor);
        if (decl_kind_ != DeclKind::Interface) {
            output_ << "public ";
        }
        output_ << "init";
        write_method_parameters(output_, constructor, format_);
        if (generate_definitions_mode() && decl_kind_ != DeclKind::Interface) {
            output_ << " { }";
        }
    }
    if (!supported) {
        output_.reset_comment();
    }
    output_ << '\n';
}

void TypeDeclarationWriter::write_instance_variable(const NonTypeSymbol& ivar)
{
    assert(ivar.is_instance_variable());
    assert(ivar.is_instance());
    auto* return_type = ivar.return_type();
    assert(return_type);
    assert(!return_type->is_unit());
    assert(ivar.is_public() || ivar.is_protected());
    const auto& name = ivar.name();
    auto supported = is_field_type_supported(decl_kind_, *return_type, name);
    if (!supported) {
        output_.set_comment();
    }
    output_ << (ivar.is_public() ? "public" : "protected") << " var " << escape_keyword(name);
    write_type(output_, ivar, *return_type, format_);
    if (generate_definitions_mode()) {
        output_ << " = " << default_value(ivar, *return_type, format_);
    }
    if (supported) {
        collect_import(*return_type);
    } else {
        output_.reset_comment();
    }
    output_ << '\n';
}

void TypeDeclarationWriter::write_field(const NonTypeSymbol& field)
{
    assert(field.is_field());
    assert(field.is_instance());
    auto* return_type = field.return_type();
    assert(return_type);
    assert(!return_type->is_unit());
    const auto& name = field.name();
    auto supported = is_field_type_supported(decl_kind_, *return_type, name);
    if (!supported) {
        output_.set_comment();
    }
    output_ << "public var " << escape_keyword(name);
    write_type(output_, field, *return_type, format_);
    if (mode != Mode::EXPERIMENTAL) {
        output_ << " = " << default_value(field, *return_type, format_);
    }
    if (supported) {
        collect_import(*return_type);
    } else {
        output_.reset_comment();
    }
    output_ << '\n';
}

void TypeDeclarationWriter::write()
{
    // Mark all classes and interfaces as @ObjCMirror.  Mark structures as @C when
    // they are empty or contain CType fields only, and as @ObjCMirror otherwise.
    // Currently FE does not support @ObjCMirror structures, so print them as
    // ordinary Cangjie structures.
    //
    // In the EXPERIMENTAL mode, print them as @ObjCMirror structures.
    //
    // In the GENERATE_DEFINITIONS mode, comment out @ObjCMirror from both
    // classes/interfaces and structures.
    switch (type_.kind()) {
        case NamedTypeSymbol::Kind::Protocol:
            decl_kind_ = DeclKind::Interface;
            format_ = SymbolPrintFormat::EmitCangjieStrict;
            print_objcmirror_attribute(output_, !generate_definitions_mode());
            break;
        case NamedTypeSymbol::Kind::Struct:
        case NamedTypeSymbol::Kind::Union:
            if (type_.is_ctype()) {
                decl_kind_ = DeclKind::CStruct;
                format_ = SymbolPrintFormat::EmitCangjie;
                output_ << "@C\n";
            } else {
                decl_kind_ = DeclKind::ObjCStruct;
                format_ = SymbolPrintFormat::EmitCangjie;
                print_objcmirror_attribute(output_, mode == Mode::EXPERIMENTAL);
            }
            break;
        default:
            assert(type_.kind() == NamedTypeSymbol::Kind::Interface);
            decl_kind_ = DeclKind::Class;
            format_ = SymbolPrintFormat::EmitCangjieStrict;
            print_objcmirror_attribute(output_, !generate_definitions_mode());
            break;
    }
    output_ << "public ";
    switch (decl_kind_) {
        case DeclKind::Interface:
            output_ << "interface";
            break;
        case DeclKind::CStruct:
        case DeclKind::ObjCStruct:
            output_ << "struct";
            break;
        default:
            assert(decl_kind_ == DeclKind::Class);
            output_ << "open class";
            break;
    }
    output_ << " " << escape_keyword(type_.name());
    if (const auto parameter_count = type_.parameter_count()) {
        output_ << "/*<";
        for (std::size_t i = 0; i < parameter_count; ++i) {
            if (i != 0) {
                output_ << ", ";
            }

            auto* parameter = type_.parameter(i);
            assert(parameter);
            output_ << parameter->name();
        }
        output_ << ">*/";
    }
    const auto base_count = type_.base_count();
    if (base_count) {
        output_ << " <: ";
        auto* base = type_.base(0);
        assert(base);
        output_ << emit_cangjie(*base);
        collect_import(*base);
        for (std::size_t i = 1; i < base_count; ++i) {
            output_ << " & ";
            auto* base = type_.base(i);
            assert(base);
            output_ << emit_cangjie(*base);
            collect_import(*base);
        }
    }
    output_ << " {\n";
    output_.indent();
    for (auto&& member : type_.members()) {
        if (member.is_property()) {
            write_property(member);
        } else if (member.is_constructor()) {
            write_constructor(member);
        } else if (member.is_member_method()) {
            if (!get_property(type_, member) &&
                !get_overridden_property(type_, member.selector(), member.is_static())) {
                write_function(output_,
                    decl_kind_ == DeclKind::Interface ? FuncKind::InterfaceMethod : FuncKind::ClassMethod, member,
                    format_);
            }
        } else if (member.is_instance_variable()) {
            write_instance_variable(member);
        } else if (member.is_field()) {
            write_field(member);
        } else {
            assert(false);
        }
    }

    // In the `GENERATE_DEFINITIONS` mode, add a fake default constructor if needed.
    // Otherwise, the following error can happen:
    // error: there is no non-parameter constructor in super class, please invoke
    // super call explicitly
    if (generate_definitions_mode() && any_constructor_exists_ && !default_constructor_exists_) {
        output_ << "public init() { }";
    }

    output_.dedent();
    output_ << "}" << std::endl;
}

static void write_enum_declaration(IndentingStringStream& output, const EnumDeclarationSymbol& enum_decl)
{
    // Can be EmitCangjieStrict, does not matter here
    auto format = SymbolPrintFormat::EmitCangjie;

    output << "public abstract sealed class " << escape_keyword(enum_decl.name()) << " {\n";
    auto& underlying_type = enum_decl.underlying_type();
    collect_import(underlying_type);
    output.indent();
    enum_decl.for_each_constant([&output, format, &underlying_type](const auto& constant) {
        output << "public static const " << escape_keyword(constant.name()) << ": "
               << SymbolPrinter(underlying_type, format) << " = ";
        print_enum_constant_value(output, underlying_type, constant);
        output << '\n';
    });
    output.dedent();
    output << '}' << std::endl;
}

void write_cangjie()
{
    std::uint64_t generated_files = 0;
    for (auto&& package : packages) {
        for (auto&& package_file : package) {
            assert(package_file.package() == &package);

            PackageFileScope scope(package_file.package()->cangjie_name());

            auto file_path = package_file.output_path();
            create_directories(file_path.parent_path());
            IndentingStringStream output;

            for (auto* symbol : package_file) {
                if (auto* alias = dynamic_cast<TypeAliasSymbol*>(symbol)) {
                    if (!write_type_alias(output, *alias)) {
                        continue;
                    }
                } else if (auto* type = dynamic_cast<TypeDeclarationSymbol*>(symbol)) {
                    TypeDeclarationWriter(output, *type).write();
                } else if (const auto* enum_decl = dynamic_cast<const EnumDeclarationSymbol*>(symbol)) {
                    write_enum_declaration(output, *enum_decl);
                } else {
                    assert(dynamic_cast<NonTypeSymbol*>(symbol));
                    auto& top_level = static_cast<NonTypeSymbol&>(*symbol);
                    assert(top_level.kind() == NonTypeSymbol::Kind::GlobalFunction);

                    // Ignore global functions with internal linkage.  Anyway, we cannot use them in
                    // Cangjie.
                    if (top_level.has_internal_linkage()) {
                        continue;
                    }

                    write_function(output, FuncKind::TopLevelFunc, top_level, SymbolPrintFormat::EmitCangjie);
                }
                output << std::endl;
            }

            std::ofstream file_output(file_path);
            file_output << "// Generated by ObjCInteropGen" << std::endl;
            file_output << std::endl;
            file_output << "package " << package.cangjie_name() << std::endl;
            file_output << std::endl;
            for (auto&& import : imports) {
                file_output << "import " << import << std::endl;
            }
            if (!generate_definitions_mode()) {
                file_output << "import interoplib.objc.*\n"
                               "import objc.lang.*\n\n";
            }
            file_output << output.str();

            generated_files++;
        }
    }

    if (generated_files == 0) {
        std::cerr << "No output files are generated" << std::endl;
    } else {
        std::cout << "Generated " << generated_files << " files for " << packages.size() << " packages" << std::endl;
    }

    if (verbosity >= LogLevel::INFO) {
        for (const auto* input_directory : inputs) {
            for (const auto* input_file : *input_directory) {
                for (const auto* symbol : *input_file) {
                    assert(symbol);
                    if (auto* package_file = symbol->package_file()) {
                        auto* edge_from = package_file->package();
                        assert(edge_from);
                        for (const auto* reference : symbol->references_symbols()) {
                            if (auto* edge_to = reference->package()) {
                                if (edge_from != edge_to) {
                                    edge_from->add_dependency_edge(edge_to);
                                }
                            }
                        }
                    }
                }
            }
        }
        for (auto&& package : packages) {
            auto& depends_on = package.depends_on();
            std::cout << "Package `" << package.cangjie_name() << "` depends on " << depends_on.size();
            if (depends_on.size() == 1) {
                auto* dependency = *depends_on.begin();
                std::cout << " package: `" << dependency->cangjie_name() << "`" << std::endl;
            } else if (depends_on.size() > 1) {
                std::cout << " packages:" << std::endl;
                for (auto* dependency : depends_on) {
                    std::cout << "* " << dependency->cangjie_name() << std::endl;
                }
            } else {
                std::cout << " packages" << std::endl;
            }
        }
    }
}

} // namespace objcgen
