// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "CangjieWriter.h"

#include <fstream>
#include <iostream>
#include <sstream>

#include "Logging.h"
#include "Mode.h"
#include "Package.h"
#include "PrintUtils.h"
#include "Strings.h"
#include "StructuredString.h"
#include "StructuredStringStream.h"
#include "Symbol.h"
#include "Universe.h"

namespace objcgen {

static void write_type_alias(StructuredString& output, const TypeAliasSymbol& alias)
{
    const auto& target = alias.target();

    auto supported = alias.is_supported();
    if (!supported) {
        output << push_line_comment;
    }
    output << "public type " << emit_cangjie(alias) << " = " << emit_cangjie(target) << '\n';
    if (!supported) {
        output << pop_line_comment;
    }
}

class DefaultValuePrinter;

static StructuredString& operator<<(StructuredString& stream, const DefaultValuePrinter& op);

class DefaultValuePrinter {
public:
    explicit DefaultValuePrinter(Printer<Type> type_printer) noexcept : type_printer_(type_printer)
    {
    }

    friend StructuredString& operator<<(StructuredString& stream, const DefaultValuePrinter& op);

private:
    const Printer<Type> type_printer_;
};

[[nodiscard]] static DefaultValuePrinter default_value(const Type& type, PrintFormat format)
{
    return DefaultValuePrinter(Printer(type, format));
}

static StructuredString& operator<<(StructuredString& stream, const DefaultValuePrinter& op)
{
    op.type_printer_.obj().print_default_value(stream, op.type_printer_.format());
    return stream;
}

static void print_enum_constant_value(
    StructuredString& output, const NamedTypeSymbol& underlying_type, const EnumConstantSymbol& constant)
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

static void write_type(StructuredString& output, const Type& type, PrintFormat format)
{
    output << ": " << Printer(type, format);
}

static void write_method_parameters(StructuredString& output, const NonTypeSymbol& method, PrintFormat format)
{
    output << '(';
    print_list(output, method.parameters(), [format](auto& output, const auto& parameter) {
        output << escape_keyword(parameter.name());
        const auto& parameter_type = parameter.type();
        write_type(output, parameter_type, format);
    });
    output << ')';
}

static void write_foreign_name(StructuredString& output, std::string_view attribute, std::string_view value)
{
    // FE supports foreign name attributes in @ObjCMirror classes only.  In the
    // GENERATE_DEFINITIONS mode, where @ObjCMirror is not used, the foreign name
    // attributes are commented out.
    auto hide_foreign_name = generate_definitions_mode();
    if (hide_foreign_name) {
        output << push_block_comment << ' ';
    }
    output << attribute << "[\"" << value << "\"]";
    if (hide_foreign_name) {
        output << ' ' << pop_block_comment;
    }
    output << ' ';
}

constexpr std::string_view foreign_name_attribute = "@ForeignName";

static void write_foreign_name(StructuredString& output, const NonTypeSymbol& method)
{
    assert(method.is_member_method() || method.is_constructor());

    // @ForeignName could not appear on overridden declaration
    if (method.is_override()) {
        return;
    }

    // Write @ForeignName only if the name of the method, as it will be written to
    // Cangjie, differs from its selector.
    const auto& selector_attribute = method.selector_attribute();
    if (!selector_attribute.empty()) {
        write_foreign_name(output, foreign_name_attribute, selector_attribute);
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
            return same_types(*t1.parameters().begin(), *t2.parameters().begin());
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
            if (t2.kind() == Type::Kind::TypeParam) {
                return &t1.actual_protocol() == &t2.actual_protocol();
            }
            return t2.kind() == Type::Kind::Named && &t1.actual_protocol() == &t2.symbol();
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

static void print_objc_optional(StructuredString& output, const NonTypeSymbol& member)
{
    if (member.is_objc_optional()) {
        if (member.is_property() && normal_mode()) {
            return;
        }
        if (member.is_override()) {
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

static void print_getter_setter_names(StructuredString& output, const NonTypeSymbol& prop)
{
    assert(prop.is_property());
    const auto& name = prop.name();
    const auto& getter_name = prop.getter();
    bool standard_getter = getter_name == name;
    if (prop.is_readonly()) {
        if (!standard_getter) {
            write_foreign_name(output, "@ForeignGetterName", getter_name);
        }
    } else {
        const auto& setter_name = prop.setter();
        if (is_standard_setter_name(name, setter_name)) {
            if (!standard_getter) {
                write_foreign_name(output, "@ForeignGetterName", getter_name);
            }
        } else if (standard_getter) {
            write_foreign_name(output, "@ForeignSetterName", setter_name);
        } else if (is_standard_setter_name(getter_name, setter_name)) {
            write_foreign_name(output, foreign_name_attribute, getter_name);
        } else {
            write_foreign_name(output, "@ForeignGetterName", getter_name);
            write_foreign_name(output, "@ForeignSetterName", setter_name);
        }
    }
}

[[nodiscard]] static bool has_name_clash_through_type_aliases(const NonTypeSymbol& symbol, const std::string& name, PrintFormat format)
{
    class Scanner : public FileLevelSymbolScanner {
    public:
        explicit Scanner(const std::string& name) noexcept : name_(name)
        {
        }

        [[nodiscard]] bool operator()(const FileLevelSymbol& symbol) const override
        {
            if (auto* type_alias = dynamic_cast<const TypeAliasSymbol*>(&symbol)) {
                if (auto& target = type_alias->target(); target.has_symbol_assigned()) {
                    auto canonical_type = type_alias->canonical_type();
                    if (canonical_type.is_ctype() && canonical_type.contains_pointer_or_func()) {
                        return (*this)(target);
                    }
                }
            }

            return name_ == symbol.name();
        }

        using FileLevelSymbolScanner::operator();

    private:
        const std::string& name_;
    };
    return mode != Mode::EXPERIMENTAL && format == PrintFormat::EmitCangjieStrict &&
        symbol.any_of_referenced_types(Scanner(name));
}

static void write_function(
    StructuredString& output, const TypeDeclarationSymbol* decl, const NonTypeSymbol& function, PrintFormat format)
{
    if (function.is_hidden()) {
        return;
    }
    const auto& return_type = function.return_type();
    const auto& name = function.name();
    auto supported = function.is_supported(decl) && !has_name_clash_through_type_aliases(function, name, format);
    if (!supported) {
        output << push_line_comment;
    }
    bool is_ctype;
    if (function.is_global_function()) {
        is_ctype = function.is_ctype();

        if (is_ctype) {
            output << "foreign ";
        } else {
            if (!generate_definitions_mode()) {
                output << "@ObjCMirror\n";
                format = PrintFormat::EmitCangjieStrict;
            }
            const auto& selector_attribute = function.selector_attribute();
            if (!selector_attribute.empty()) {
                write_foreign_name(output, foreign_name_attribute, selector_attribute);
            }
            output << "public ";
        }
    } else if (function.is_protocol_method()) {
        is_ctype = false;
        print_objc_optional(output, function);
        write_foreign_name(output, function);
    } else if (function.is_interface_method()) {
        is_ctype = false;
        write_foreign_name(output, function);
        output << "public ";
    } else {
        assert(false);
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
        if (function.is_interface_method()) {
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
    output << "func " << escape_keyword(name);
    write_method_parameters(output, function, format);
    write_type(output, return_type, format);
    if (generate_definitions_mode() && !is_ctype) {
        if (return_type.is_unit()) {
            output << " { }";
        } else {
            output << " { " << default_value(return_type, format) << " }";
        }
    }
    if (!supported) {
        output << pop_line_comment;
    }
    output << '\n';
}

static void print_objcmirror_attribute(StructuredString& output, const NamedTypeSymbol& decl, bool supported)
{
    auto hide_objcmirror_attribute = !supported;
    if (hide_objcmirror_attribute) {
        output << push_block_comment << ' ';
    }
    output << "@ObjCMirror";
    const auto& objc_name_attribute = decl.objc_name_attribute();
    if (!objc_name_attribute.empty()) {
        output << "[\"" << objc_name_attribute << "\"]";
    }
    if (hide_objcmirror_attribute) {
        output << ' ' << pop_block_comment;
    }
    output << '\n';
}

class TypeDeclarationWriter final {
public:
    TypeDeclarationWriter(StructuredString& output, TypeDeclarationSymbol& decl) noexcept;

    void write();

private:
    void write_property(const NonTypeSymbol& prop);
    void write_constructor(NonTypeSymbol& constructor);
    void write_instance_variable(NonTypeSymbol& ivar);
    void write_field(const NonTypeSymbol& field);

    [[nodiscard]] bool is_interface() const noexcept
    {
        return decl_.kind() == NamedTypeSymbol::Kind::Protocol;
    }

    StructuredString& output_;
    TypeDeclarationSymbol& decl_;
    PrintFormat format_;
    bool any_constructor_exists_ = false;
    bool default_constructor_exists_ = false;
};

TypeDeclarationWriter::TypeDeclarationWriter(StructuredString& output, TypeDeclarationSymbol& decl) noexcept
    : output_(output), decl_(decl)
{
}

void TypeDeclarationWriter::write_property(const NonTypeSymbol& prop)
{
    assert(prop.is_property());
    auto is_static = prop.is_static();
    auto getter = prop.find_getter(decl_);
    auto supported = prop.is_supported(&decl_) && !has_name_clash_through_type_aliases(*getter, prop.name(), format_);
    if (!supported) {
        output_ << push_line_comment;
    }

    // Only interfaces can have @ObjCOptional members, not classes
    assert(!prop.is_objc_optional() || is_interface());
    if (is_interface()) {
        print_objc_optional(output_, prop);
    }

    print_getter_setter_names(output_, prop);
    if (!is_interface()) {
        output_ << "public ";
    }
    if (is_static) {
        output_ << "static ";
    } else if (!is_interface()) {
        output_ << "open ";
    }
    if (!prop.is_readonly()) {
        output_ << "mut ";
    }
    const auto& return_type = prop.property_type(decl_);
    ;
    assert(!return_type.is_unit());
    output_ << "prop " << escape_keyword(prop.name());
    write_type(output_, return_type, format_);
    if (generate_definitions_mode()) {
        BraceScope scope(output_);
        output_ << "get() { " << default_value(return_type, format_) << " }\n";
        if (!prop.is_readonly()) {
            output_ << "set(v) { }\n";
        }
    }
    if (!supported) {
        output_ << pop_line_comment;
    }
    output_ << '\n';
}

void TypeDeclarationWriter::write_constructor(NonTypeSymbol& constructor)
{
    assert(constructor.is_constructor());
    auto supported = constructor.is_supported(&decl_);
    if (supported) {
        any_constructor_exists_ = true;
        if (!default_constructor_exists_) {
            default_constructor_exists_ = constructor.parameter_count() == 0;
        }
    } else {
        output_ << push_line_comment;
    }
    if (is_overloading_constructor(decl_, constructor)) {
        if (!generate_definitions_mode()) {
            output_ << "@ObjCInit ";
        }

        // The constructor will be written as a static method with its original name.
        write_foreign_name(output_, constructor);

        if (!is_interface()) {
            output_ << "public ";
        }
        output_ << "static func " << escape_keyword(constructor.name());
        write_method_parameters(output_, constructor, format_);

        const auto& return_type = constructor.return_type();
        write_type(output_, return_type, format_);
        if (generate_definitions_mode() && !is_interface()) {
            output_ << " { " << default_value(return_type, format_) << " }";
        }
    } else {
        // The constructor will be written with the name 'init'.
        // Write @ForeignName only if the selector is different.
        constexpr std::string_view default_constructor_name = "init";
        const auto& selector = constructor.selector();
        if (selector != default_constructor_name) {
            write_foreign_name(output_, foreign_name_attribute, selector);
        }

        if (!is_interface()) {
            output_ << "public ";
        }
        output_ << default_constructor_name;
        write_method_parameters(output_, constructor, format_);
        if (generate_definitions_mode() && !is_interface()) {
            output_ << " { }";
        }
    }
    if (!supported) {
        output_ << pop_line_comment;
    }
    output_ << '\n';
}

void TypeDeclarationWriter::write_instance_variable(NonTypeSymbol& ivar)
{
    assert(ivar.is_instance_variable());
    assert(ivar.is_instance());
    assert(ivar.is_public() || ivar.is_protected());
    const auto& name = ivar.name();
    auto supported = ivar.is_supported(&decl_) && !has_name_clash_through_type_aliases(ivar, ivar.name(), format_);
    if (!supported) {
        output_ << push_line_comment;
    }
    const auto& selector_attribute = ivar.selector_attribute();
    if (!selector_attribute.empty()) {
        write_foreign_name(output_, foreign_name_attribute, selector_attribute);
    }
    output_ << (ivar.is_public() ? "public" : "protected") << " var " << escape_keyword(name);
    const auto& return_type = ivar.return_type();
    assert(!return_type.is_unit());
    write_type(output_, return_type, format_);
    if (generate_definitions_mode()) {
        output_ << " = " << default_value(return_type, format_);
    }
    if (!supported) {
        output_ << pop_line_comment;
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
    auto supported = field.is_supported(&decl_) && !has_name_clash_through_type_aliases(field, field.name(), format_);
    if (!supported) {
        output_ << push_line_comment;
    }
    output_ << "public var " << escape_keyword(field.name());
    const auto& return_type = field.return_type();
    assert(!return_type.is_unit());
    write_type(output_, return_type, format_);
    if (mode != Mode::EXPERIMENTAL) {
        output_ << " = " << default_value(return_type, format_);
    }
    if (!supported) {
        output_ << pop_line_comment;
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
        case NamedTypeSymbol::Kind::Interface:
            format_ = PrintFormat::EmitCangjieStrict;
            print_objcmirror_attribute(output_, decl_, !generate_definitions_mode());
            break;
        case NamedTypeSymbol::Kind::Struct:
        case NamedTypeSymbol::Kind::Union:
            format_ = PrintFormat::EmitCangjie;
            if (decl_.is_ctype()) {
                output_ << "@C\n";
            } else {
                print_objcmirror_attribute(output_, decl_, mode == Mode::EXPERIMENTAL);
            }
            break;
        default:
            assert(false);
            return;
    }
    output_ << "public ";
    switch (decl_.kind()) {
        case NamedTypeSymbol::Kind::Protocol:
            output_ << "interface";
            break;
        case NamedTypeSymbol::Kind::Struct:
        case NamedTypeSymbol::Kind::Union:
            output_ << "struct";
            break;
        case NamedTypeSymbol::Kind::Interface:
            output_ << "open class";
            break;
        default:
            assert(false);
            return;
    }
    output_ << ' ' << escape_keyword(decl_.name());
    auto parameters = decl_.parameters();
    if (!parameters.empty()) {
        output_ << push_block_comment << '<';
        print_list(output_, parameters, [](auto& output, const auto& parameter) { output << emit_cangjie(parameter); });
        output_ << '>' << pop_block_comment;
    }
    auto bases = decl_.bases();
    if (!bases.empty()) {
        output_ << " <: ";
        print_list(output_, bases, [](auto& output, auto& base) { output << emit_cangjie(base); }, " & ");
    }
    {
        BraceScope scope(output_);
        for (auto&& member : decl_.members()) {
            if (member.is_hidden()) {
                continue;
            }
            auto closure_depth = Config::closure_depth();
            if (closure_depth < UNLIMITED_CLOSURE_DEPTH && member.calculate_reference_level(decl_) > closure_depth) {
                continue;
            }
            if (member.is_property()) {
                write_property(member);
            } else if (member.is_constructor()) {
                write_constructor(member);
            } else if (member.is_member_method()) {
                write_function(output_, &decl_, member, format_);
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
            output_ << "public init() { }\n";
        }
    }
    output_ << '\n';
}

static void write_enum_declaration(StructuredString& output, const EnumDeclarationSymbol& enum_decl)
{
    // Can be emit_cangjie_strict, does not matter here
    auto enum_decl_printer = emit_cangjie(enum_decl);

    const auto& underlying_type = enum_decl.underlying_type();
    output << "public type " << emit_cangjie(enum_decl) << " = " << emit_cangjie(underlying_type) << '\n';
    enum_decl.for_each_constant([&output, &enum_decl_printer, &underlying_type](const auto& constant) {
        output << "public const " << escape_keyword(constant.name()) << ": " << enum_decl_printer << " = ";
        print_enum_constant_value(output, underlying_type, constant);
        output << '\n';
    });
}

[[nodiscard]] static std::string assemble_file_content(
    const Package& package, const std::set<std::string>& imports, std::string_view body)
{
    std::ostringstream file_output;
    file_output << "// Generated by ObjCInteropGen" << std::endl;
    file_output << std::endl;
    file_output << "package " << package.cangjie_name() << std::endl;
    file_output << std::endl;
    for (const auto& import : imports) {
        file_output << "import " << import << std::endl;
    }
    if (!generate_definitions_mode()) {
        file_output << "import objc.lang.*\n\n";
    }
    file_output << body;
    return file_output.str();
}

void write_cangjie()
{
    std::uint64_t generated_files = 0;
    for (auto&& package : packages) {
        for (auto&& package_file : package) {
            assert(&package_file.package() == &package);

            const auto& file_path = package_file.output_path();
            create_directories(file_path.parent_path());
            StructuredString output;

            for (auto* symbol : package_file) {
                if (auto* alias = dynamic_cast<TypeAliasSymbol*>(symbol)) {
                    write_type_alias(output, *alias);
                } else if (auto* type = dynamic_cast<TypeDeclarationSymbol*>(symbol)) {
                    TypeDeclarationWriter(output, *type).write();
                } else if (const auto* enum_decl = dynamic_cast<const EnumDeclarationSymbol*>(symbol)) {
                    write_enum_declaration(output, *enum_decl);
                } else {
                    auto& top_level = symbol->as<NonTypeSymbol>();
                    assert(top_level.is_global_function());
                    write_function(output, nullptr, top_level, PrintFormat::EmitCangjie);
                }
                output << '\n';
            }

            StructuredStringStream rendered(package);
            rendered << output;

            const auto file_content = assemble_file_content(package, rendered.imports(), rendered.str());

            std::string existing_content;
            if (std::ifstream existing(file_path); existing) {
                existing_content.assign(std::istreambuf_iterator(existing), std::istreambuf_iterator<char>());
            }
            // Skip writing when unchanged to preserve the file modification time.
            if (existing_content != file_content) {
                std::ofstream file_output(file_path);
                file_output << file_content;
            }

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
