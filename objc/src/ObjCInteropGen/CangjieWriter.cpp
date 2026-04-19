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
#include "PrintUtils.h"
#include "Strings.h"
#include "Symbol.h"
#include "SymbolVisitor.h"
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

class PackageFileScope final : private NonCopyable {
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
};

[[nodiscard]] static std::string symbol_to_import_name(const FileLevelSymbol& symbol)
{
    assert(!current_package_name.empty());
    const auto& symbol_package_name = symbol.cangjie_package_name();
    if (symbol_package_name.empty() || symbol_package_name == current_package_name) {
        return {};
    }
    return symbol_package_name + '.' + symbol.name();
}

class ImportCollectVisitor final : public SymbolVisitor {
public:
    ImportCollectVisitor() noexcept : SymbolVisitor(false)
    {
    }

private:
    void visit_impl(const Type& value)
    {
        visit_impl(value.symbol());
    }

    void visit_type_impl(const Type& value) override
    {
        visit_impl(value);
    }

    void visit_type_impl(const NamedTypeSymbol& value) override
    {
        visit_impl(value);
    }

    void visit_type_argument_impl(const TypeLikeSymbol& owner, const Type& value) override
    {
        // If this is a type argument of an Objective-C generic type, ignore it.  Type
        // arguments are erased and may be printed inside comments only.
        if (owner.is<TypeDeclarationSymbol>()) {
            return;
        }

        visit_impl(value);
    }

    void visit_member_impl(const NonTypeSymbol& value) override
    {
        visit_impl(value);
    }

    void visit_impl(const FileLevelSymbol& symbol) override;
};

void ImportCollectVisitor::visit_impl(const FileLevelSymbol& symbol)
{
    auto import_name = symbol_to_import_name(symbol);
    if (!import_name.empty()) {
        imports.emplace(std::move(import_name));
    }
}

static void collect_import(const TypeLikeSymbol& symbol)
{
    ImportCollectVisitor().visit(symbol);
}

static void collect_import(const Type& type)
{
    ImportCollectVisitor().visit(type);
}

// Currently in the NORMAL mode, Objective-C compatible types are primitives,
// @C structures, ObjCPointer, ObjCFunc, ObjCBlock, and classes/interfaces.
// But not CPointer, CFunc, or VArray.
static bool is_objc_compatible(const Type& type)
{
    assert(normal_mode());
    switch (type.kind()) {
        case Type::Kind::Unit:
            return true;
        case Type::Kind::TypeParam:
            // Type parameters are printed as ObjCId, which is Objective-C compatible
            return true;
        case Type::Kind::Pointer:
            assert(type.parameters().size() == 1);
            return is_objc_compatible(type.parameters().front());
        case Type::Kind::Function:
        case Type::Kind::Block: {
            const auto& parameters = type.parameters();
            return std::all_of(parameters.begin(), parameters.end(),
                [](const auto& parameter) { return is_objc_compatible(parameter); });
        }
        case Type::Kind::Named: {
            const auto& type_symbol = type.symbol();
            if (&type_symbol == &Universe::get().sel()) {
                return false;
            }
            switch (type_symbol.as<NamedTypeSymbol>().kind()) {
                case NamedTypeSymbol::Kind::TypeDef:
                    return is_objc_compatible(type_symbol.as<TypeAliasSymbol>().canonical_type());
                case NamedTypeSymbol::Kind::Struct:
                case NamedTypeSymbol::Kind::Union:
                    return type_symbol.is_ctype();
                case NamedTypeSymbol::Kind::Interface:
                    return type_symbol.name() != "Protocol";
                case NamedTypeSymbol::Kind::Primitive:
                case NamedTypeSymbol::Kind::Protocol:
                case NamedTypeSymbol::Kind::Enum:
                    return true;
                default:
                    return false;
            }
        }
        default:
            return false;
    }
}

[[nodiscard]] static bool write_type_alias(IndentingStringStream& output, const TypeAliasSymbol& alias)
{
    const auto& target = alias.target();
    if (alias.name() == target.name()) {
        // typedef struct S S;
        // In the Cangjie output, the target symbol is used directly instead of this typedef, do not print it.
        return false;
    }

    auto supported = !normal_mode() || target.is_ctype() || is_objc_compatible(target);
    if (supported) {
        collect_import(target);
    } else {
        output.set_comment();
    }
    output << "public type " << emit_cangjie(alias) << " = " << emit_cangjie(target) << '\n';
    if (!supported) {
        output.reset_comment();
    }
    return true;
}

class DefaultValuePrinter;

static std::ostream& operator<<(std::ostream& stream, const DefaultValuePrinter& op);

class DefaultValuePrinter {
public:
    explicit DefaultValuePrinter(Printer<Type> type_printer) noexcept : type_printer_(type_printer)
    {
    }

    friend std::ostream& operator<<(std::ostream& stream, const DefaultValuePrinter& op);

private:
    const Printer<Type> type_printer_;
};

[[nodiscard]] static DefaultValuePrinter default_value(const Type& type, PrintFormat format)
{
    return DefaultValuePrinter(Printer(type, format));
}

static std::ostream& operator<<(std::ostream& stream, const DefaultValuePrinter& op)
{
    op.type_printer_.obj().print_default_value(stream, op.type_printer_.format());
    return stream;
}

static void print_enum_constant_value(
    std::ostream& output, const NamedTypeSymbol& underlying_type, const EnumConstantSymbol& constant)
{
    const auto& canonical_type_symbol = underlying_type.kind() == NamedTypeSymbol::Kind::TypeDef
        ? underlying_type.as<TypeAliasSymbol>().canonical_type_symbol().as<NamedTypeSymbol>()
        : underlying_type;
    if (canonical_type_symbol.kind() == NamedTypeSymbol::Kind::Primitive) {
        const auto& primitive_type = canonical_type_symbol.as<PrimitiveTypeSymbol>();
        // Avoid the "number exceeds the value range of type" Cangjie compiler error by
        // printing the value in a type-specific way.
        auto category = primitive_type.category();
        auto size = primitive_type.size();
        if (category == PrimitiveTypeCategory::SignedInteger) {
            switch (size) {
                case PrimitiveSize::One:
                    output << static_cast<int>(constant.value<int8_t>());
                    return;
                case PrimitiveSize::Two:
                    output << constant.value<int16_t>();
                    return;
                case PrimitiveSize::Four:
                    output << constant.value<int32_t>();
                    return;
                default:
                    assert(size == PrimitiveSize::Eight);
                    output << constant.value<int64_t>();
                    return;
            }
        }
        assert(category == PrimitiveTypeCategory::UnsignedInteger);
        switch (size) {
            case PrimitiveSize::One:
                output << static_cast<uint32_t>(constant.value<uint8_t>());
                return;
            case PrimitiveSize::Two:
                output << constant.value<uint16_t>();
                return;
            case PrimitiveSize::Four:
                output << constant.value<uint32_t>();
                return;
            default:
                assert(size == PrimitiveSize::Eight);
                output << constant.value<uint64_t>();
                return;
        }
    }

    // This is _int128 or unsigned _int128 represented respectively as
    // VArray<Int64, $2> or VArray<UInt64, $2>
    assert(canonical_type_symbol.kind() == NamedTypeSymbol::Kind::Unexposed);
    const auto& type = canonical_type_symbol.as<UnexposedTypeSymbol>().underlying_type();
    assert(type.kind() == Type::Kind::VArray);
    assert(type.varray_size() == 2);
    const auto& element_type = type.varray_element_type().symbol();
    auto& universe = Universe::get();
    output << '[';
    if (&element_type == &universe.int64()) {
        output << constant.value128_lo<int64_t>() << ", " << constant.value128_hi<int64_t>();
    } else {
        assert(&element_type == &universe.uint64());
        output << constant.value128_lo<uint64_t>() << ", " << constant.value128_hi<uint64_t>();
    }
    output << ']';
}

[[nodiscard]] static bool is_objc_compatible_parameters(const NonTypeSymbol& method) noexcept
{
    for (const auto& parameter : method.parameters()) {
        if (!is_objc_compatible(parameter.type())) {
            return false;
        }
    }
    return true;
}

static void write_type(std::ostream& output, const Type& type, PrintFormat format)
{
    output << ": " << Printer(type, format);
}

static void write_method_parameters(std::ostream& output, const NonTypeSymbol& method, PrintFormat format)
{
    output << '(';
    print_list(output, method.parameters(), [format](auto& output, const auto& parameter) {
        output << escape_keyword(parameter.name());
        const auto& parameter_type = parameter.type();
        write_type(output, parameter_type, format);
        collect_import(parameter_type);
    });
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

[[nodiscard]] static bool same_types(const Type& type1, const Type& type2) noexcept
{
    const auto* symbol1 = &type1.symbol();
    const auto* symbol2 = &type2.symbol();
    const auto* alias = dynamic_cast<const TypeAliasSymbol*>(symbol1);
    Type t1;
    if (alias) {
        t1 = alias->canonical_type();
        symbol1 = &t1.symbol();
    } else {
        t1 = type1;
    }
    alias = dynamic_cast<const TypeAliasSymbol*>(symbol2);
    Type t2;
    if (alias) {
        t2 = alias->canonical_type();
        symbol2 = &t2.symbol();
    } else {
        t2 = type2;
    }

    if (t1.is_cj_direct_option() != t2.is_cj_direct_option()) {
        return false;
    }

    switch (t1.kind()) {
        case Type::Kind::Pointer:
            if (t2.kind() != Type::Kind::Pointer) {
                return false;
            }
            assert(t1.parameters().size() == 1);
            assert(t2.parameters().size() == 1);
            return same_types(t1.parameters().front(), t2.parameters().front());
        case Type::Kind::Function: {
            if (t2.kind() != Type::Kind::Function) {
                return false;
            }
            const auto& parameters1 = t1.parameters();
            const auto& parameters2 = t2.parameters();
            return std::equal(parameters1.begin(), parameters1.end(), parameters2.begin(), parameters2.end(),
                [](const auto& param1, const auto& param2) { return same_types(param1, param2); });
        }
        case Type::Kind::VArray:
            return t2.kind() == Type::Kind::VArray && t1.varray_size() == t2.varray_size() &&
                same_types(t1.varray_element_type(), t2.varray_element_type());
        case Type::Kind::TypeParam:
            return t2.kind() == Type::Kind::TypeParam && &t1.actual_protocol() == &t2.actual_protocol();
        default:
            return symbol1 == symbol2;
    }
}

[[nodiscard]] static bool is_overloading_constructor(
    const TypeDeclarationSymbol& type, const NonTypeSymbol& constructor)
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

[[nodiscard]] static NonTypeSymbol* get_overridden_method(const TypeDeclarationSymbol& decl, const NonTypeSymbol& prop)
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

[[nodiscard]] static const NonTypeSymbol* get_property(
    const TypeDeclarationSymbol& decl, const NonTypeSymbol& getter_or_setter)
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

[[nodiscard]] static const NonTypeSymbol* get_method_by_selector(
    const TypeDeclarationSymbol& decl, const std::string& selector, bool is_static)
{
    for (auto& member : decl.members()) {
        if (member.is_member_method() && member.is_static() == is_static && member.selector() == selector) {
            return &member;
        }
    }
    return nullptr;
}

[[nodiscard]] static NonTypeSymbol* get_overridden_property(
    const TypeDeclarationSymbol& decl, const std::string& getter, bool is_static)
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

static void print_objc_optional(std::ostream& output, const NonTypeSymbol& member)
{
    if (member.is_objc_optional()) {
        if (member.is_property() && normal_mode()) {
            return;
        }
        if (generate_definitions_mode()) {
            output << "// ";
        }
        output << "@ObjCOptional\n";
    }
}

[[nodiscard]] static bool is_standard_setter_name(std::string_view prop_name, std::string_view setter_name)
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
    IndentingStringStream& output, FuncKind kind, const NonTypeSymbol& function, PrintFormat format)
{
    const auto& return_type = function.return_type();
    auto supported = !normal_mode() || (is_objc_compatible(return_type) && is_objc_compatible_parameters(function));
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
                    format = PrintFormat::EmitCangjieStrict;
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
            print_objc_optional(output, function);
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
    write_type(output, return_type, format);
    if (generate_definitions_mode() && !is_ctype) {
        if (return_type.is_unit()) {
            output << " { }";
        } else {
            output << " { " << default_value(return_type, format) << " }";
        }
    }
    if (supported) {
        collect_import(return_type);
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
[[nodiscard]] static bool is_field_type_supported(DeclKind decl_kind, const Type& type, const std::string& name)
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

class TypeDeclarationWriter final {
public:
    TypeDeclarationWriter(IndentingStringStream& output, TypeDeclarationSymbol& decl) noexcept;

    void write();

private:
    void write_property(const NonTypeSymbol& prop);
    void write_constructor(NonTypeSymbol& constructor);
    void write_instance_variable(const NonTypeSymbol& ivar);
    void write_field(const NonTypeSymbol& field);

    IndentingStringStream& output_;
    TypeDeclarationSymbol& decl_;
    DeclKind decl_kind_;
    PrintFormat format_;
    bool any_constructor_exists_ = false;
    bool default_constructor_exists_ = false;
};

TypeDeclarationWriter::TypeDeclarationWriter(IndentingStringStream& output, TypeDeclarationSymbol& decl) noexcept
    : output_(output), decl_(decl)
{
}

void TypeDeclarationWriter::write_property(const NonTypeSymbol& prop)
{
    assert(prop.is_property());
    auto is_static = prop.is_static();
    if (get_overridden_method(decl_, prop)) {
        return;
    }
    const auto& getter_name = prop.getter();
    if (get_overridden_property(decl_, getter_name, is_static)) {
        return;
    }
    auto* getter = get_method_by_selector(decl_, getter_name, is_static);
    assert(getter);
    const auto& return_type = getter->return_type();
    assert(!return_type.is_unit());
    const auto& name = prop.name();
    auto supported = is_field_type_supported(decl_kind_, return_type, name);
    if (!supported) {
        output_.set_comment();
    }

    // Only interfaces can have @ObjCOptional members, not classes
    assert(!prop.is_objc_optional() || decl_kind_ == DeclKind::Interface);
    if (decl_kind_ == DeclKind::Interface) {
        print_objc_optional(output_, prop);
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
    write_type(output_, return_type, format_);
    if (generate_definitions_mode()) {
        output_ << " {\n";
        output_.indent();
        output_ << "get() { " << default_value(return_type, format_) << " }\n";
        if (!prop.is_readonly()) {
            output_ << "set(v) { }\n";
        }
        output_.dedent();
        output_ << '}';
    }
    if (supported) {
        collect_import(return_type);
    } else {
        output_.reset_comment();
    }
    output_ << '\n';
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
    if (is_overloading_constructor(decl_, constructor)) {
        if (!generate_definitions_mode()) {
            output_ << "@ObjCInit ";
        }
        write_foreign_name(output_, constructor);
        if (decl_kind_ != DeclKind::Interface) {
            output_ << "public ";
        }
        output_ << "static func " << escape_keyword(constructor.name());
        write_method_parameters(output_, constructor, format_);

        // FE requires the return type to be strictly the declaring class, and the
        // nullability must be nonnull
        if (normal_mode()) {
            std::vector<Type> args;
            args.reserve(decl_.parameter_count());
            for (auto& parameter : decl_.parameters()) {
                args.emplace_back(parameter, Nullability::Nonnull);
            }
            Type return_type(decl_, std::move(args), Nullability::Nonnull);

            write_type(output_, return_type, format_);
            if (supported) {
                collect_import(return_type);
            }
        } else {
            auto return_type = constructor.return_type();
            return_type.set_nullability(Nullability::Nonnull);

            write_type(output_, return_type, format_);
            if (generate_definitions_mode() && decl_kind_ != DeclKind::Interface) {
                output_ << " { " << default_value(return_type, format_) << " }";
            }
            if (supported) {
                collect_import(return_type);
            }
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
    const auto& return_type = ivar.return_type();
    assert(!return_type.is_unit());
    assert(ivar.is_public() || ivar.is_protected());
    const auto& name = ivar.name();
    auto supported = is_field_type_supported(decl_kind_, return_type, name);
    if (!supported) {
        output_.set_comment();
    }
    output_ << (ivar.is_public() ? "public" : "protected") << " var " << escape_keyword(name);
    write_type(output_, return_type, format_);
    if (generate_definitions_mode()) {
        output_ << " = " << default_value(return_type, format_);
    }
    if (supported) {
        collect_import(return_type);
    } else {
        output_.reset_comment();
    }
    output_ << '\n';
}

void TypeDeclarationWriter::write_field(const NonTypeSymbol& field)
{
    assert(field.is_field());
    assert(field.is_instance());
    if (field.is_bit_field() && field.name().empty()) {
        return;
    }
    const auto& return_type = field.return_type();
    assert(!return_type.is_unit());
    const auto& name = field.name();
    auto supported = is_field_type_supported(decl_kind_, return_type, name);
    if (!supported) {
        output_.set_comment();
    }
    output_ << "public var " << escape_keyword(name);
    write_type(output_, return_type, format_);
    if (mode != Mode::EXPERIMENTAL) {
        output_ << " = " << default_value(return_type, format_);
    }
    if (supported) {
        collect_import(return_type);
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
    switch (decl_.kind()) {
        case NamedTypeSymbol::Kind::Protocol:
            decl_kind_ = DeclKind::Interface;
            format_ = PrintFormat::EmitCangjieStrict;
            print_objcmirror_attribute(output_, !generate_definitions_mode());
            break;
        case NamedTypeSymbol::Kind::Struct:
        case NamedTypeSymbol::Kind::Union:
            if (decl_.is_ctype()) {
                decl_kind_ = DeclKind::CStruct;
                format_ = PrintFormat::EmitCangjie;
                output_ << "@C\n";
            } else {
                decl_kind_ = DeclKind::ObjCStruct;
                format_ = PrintFormat::EmitCangjie;
                print_objcmirror_attribute(output_, mode == Mode::EXPERIMENTAL);
            }
            break;
        default:
            assert(decl_.kind() == NamedTypeSymbol::Kind::Interface);
            decl_kind_ = DeclKind::Class;
            format_ = PrintFormat::EmitCangjieStrict;
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
    output_ << ' ' << escape_keyword(decl_.name());
    auto parameters = decl_.parameters();
    if (!parameters.empty()) {
        output_ << "/*<";
        print_list(output_, parameters, [](auto& output, const auto& parameter) { output << emit_cangjie(parameter); });
        output_ << ">*/";
    }
    auto bases = decl_.bases();
    if (!bases.empty()) {
        output_ << " <: ";
        print_list(
            output_, bases,
            [](auto& output, auto& base) {
                output << emit_cangjie(base);
                collect_import(base);
            },
            " & ");
    }
    output_ << " {\n";
    output_.indent();
    for (auto&& member : decl_.members()) {
        if (member.is_property()) {
            write_property(member);
        } else if (member.is_constructor()) {
            write_constructor(member);
        } else if (member.is_member_method()) {
            if (!get_property(decl_, member) &&
                !get_overridden_property(decl_, member.selector(), member.is_static())) {
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
    output_ << '}' << std::endl;
}

static void write_enum_declaration(IndentingStringStream& output, const EnumDeclarationSymbol& enum_decl)
{
    // Can be EmitCangjieStrict, does not matter here
    auto format = PrintFormat::EmitCangjie;

    const auto& underlying_type = enum_decl.underlying_type();
    collect_import(underlying_type);
    output << "public type " << emit_cangjie(enum_decl) << " = " << emit_cangjie(underlying_type) << '\n';
    enum_decl.for_each_constant([&output, format, &enum_decl, &underlying_type](const auto& constant) {
        output << "public const " << escape_keyword(constant.name()) << ": " << Printer(enum_decl, format) << " = ";
        print_enum_constant_value(output, underlying_type, constant);
        output << '\n';
    });
}

void write_cangjie()
{
    std::uint64_t generated_files = 0;
    for (auto&& package : packages) {
        for (auto&& package_file : package) {
            assert(&package_file.package() == &package);

            PackageFileScope scope(package_file.package().cangjie_name());

            const auto& file_path = package_file.output_path();
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
                    auto& top_level = symbol->as<NonTypeSymbol>();
                    assert(top_level.kind() == NonTypeSymbol::Kind::GlobalFunction);

                    // Ignore global functions with internal linkage.  Anyway, we cannot use them in
                    // Cangjie.
                    if (top_level.has_internal_linkage()) {
                        continue;
                    }

                    write_function(output, FuncKind::TopLevelFunc, top_level, PrintFormat::EmitCangjie);
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
                file_output << "import objc.lang.*\n\n";
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
        for (const auto& input_file : inputs) {
            for (const auto& symbol : input_file) {
                if (auto* package_file = symbol.package_file()) {
                    auto& edge_from = package_file->package();
                    for (const auto* reference : symbol.references_symbols()) {
                        if (auto* edge_to = reference->package()) {
                            if (&edge_from != edge_to) {
                                edge_from.add_dependency_edge(*edge_to);
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
                std::cout << " package: `" << dependency->cangjie_name() << '`' << std::endl;
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
