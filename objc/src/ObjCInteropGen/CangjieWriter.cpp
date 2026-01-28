// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "CangjieWriter.h"
#include "Logging.h"

#include <fstream>
#include <iostream>
#include <set>
#include <sstream>

#include "Logging.h"
#include "Mode.h"
#include "Package.h"
#include "SingleDeclarationSymbolVisitor.h"
#include "Strings.h"

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

std::string_view current_package_name;
std::set<std::string> imports;

class PackageFileScope final {
    const std::string_view package_name_;

public:
    [[nodiscard]] explicit PackageFileScope(std::string_view package_name) noexcept : package_name_(package_name_)
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

static bool is_objc_compatible_type(TypeLikeSymbol& type) noexcept
{
    assert(normal_mode());
    auto* canonical_type = dynamic_cast<NamedTypeSymbol*>(&type.canonical_type());
    if (!canonical_type) {
        return false;
    }
    switch (canonical_type->kind()) {
        case NamedTypeSymbol::Kind::TargetPrimitive:
        case NamedTypeSymbol::Kind::Enum: {
            return true;
        }
        case NamedTypeSymbol::Kind::Struct:
            return canonical_type->is_ctype();
        case NamedTypeSymbol::Kind::Protocol:
            return false;
        default:
            return true;
    }
}

// Currently in the NORMAL mode, `ObjCPointer` supports only primitives,
// `ObjCPointer` itself, and classes as its type parameter.
static bool is_objc_compatible_objcpointer_pointee(const NamedTypeSymbol& pointee)
{
    assert(normal_mode());
    switch (pointee.kind()) {
        case NamedTypeSymbol::Kind::Struct: {
            if (pointee.name() != "ObjCPointer") {
                return false;
            }
            assert(pointee.parameter_count() == 1);
            auto* p = pointee.parameter(0);
            if (!p) {
                return false;
            }
            const auto* canonical_p = dynamic_cast<const NamedTypeSymbol*>(&p->canonical_type());
            return canonical_p && is_objc_compatible_objcpointer_pointee(*canonical_p);
        }
        case NamedTypeSymbol::Kind::TargetPrimitive: {
            const auto& name = pointee.name();
            return name != "CPointer" && name != "CFunc";
        }
        case NamedTypeSymbol::Kind::Interface:
            return true;
        default:
            return false;
    }
}

static bool is_objc_compatible_parameter_type(TypeLikeSymbol& type)
{
    assert(normal_mode());
    auto* canonical_type = dynamic_cast<NamedTypeSymbol*>(&type.canonical_type());
    if (!canonical_type) {
        return false;
    }
    switch (canonical_type->kind()) {
        case NamedTypeSymbol::Kind::TargetPrimitive:
        case NamedTypeSymbol::Kind::Enum: {
            auto name = canonical_type->name();
            return name != "CPointer" && name != "CFunc";
        }
        case NamedTypeSymbol::Kind::Struct: {
            if (canonical_type->is_ctype()) {
                return true;
            }
            if (canonical_type->name() != "ObjCPointer") {
                return false;
            }
            assert(canonical_type->parameter_count() == 1);
            const auto* pointee = canonical_type->parameter(0);
            assert(pointee);
            const auto* named_pointee = dynamic_cast<const NamedTypeSymbol*>(pointee);
            if (named_pointee) {
                return is_objc_compatible_objcpointer_pointee(*named_pointee);
            }
            return false;
        }
        case NamedTypeSymbol::Kind::Protocol:
            return false;
        default: {
            if (canonical_type->is_ctype()) {
                return false;
            }
            auto name = canonical_type->name();
            return name != "SEL" && name != "Class" && name != "Protocol";
        }
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

    auto hidden = normal_mode() && !is_objc_compatible_type(alias);
    if (hidden) {
        output.set_comment();
    } else {
        collect_import(*target);
    }
    output << "public type " << emit_cangjie(alias) << " = " << emit_cangjie(target) << '\n';
    if (hidden) {
        output.reset_comment();
    }
    return true;
}

class DefaultValuePrinter {
public:
    explicit DefaultValuePrinter(const TypeLikeSymbol& type) noexcept : type(type)
    {
    }

    friend std::ostream& operator<<(std::ostream& stream, const DefaultValuePrinter& op);

private:
    const TypeLikeSymbol& type;
};

static DefaultValuePrinter default_value(const TypeLikeSymbol& symbol)
{
    return DefaultValuePrinter(symbol);
}

static bool is_integer_type(std::string_view type_name)
{
    return type_name == "Int32" || type_name == "UInt32" || type_name == "Int64" || type_name == "UInt64" ||
        type_name == "Int16" || type_name == "UInt16" || type_name == "Int8" || type_name == "UInt8";
}

// The dirty trick is applied for printing default values of:
// - Interface types -- instances of the interface type cannot be created.
// - @ObjCMirror classes -- they do not have a primary constructor.
static void print_tricky_default_value(std::ostream& stream, std::string_view type_name)
{
    stream << "Option<" << type_name << ">.None.getOrThrow()";
}

std::ostream& operator<<(std::ostream& stream, const DefaultValuePrinter& op)
{
    if (dynamic_cast<const TypeParameterSymbol*>(&op.type)) {
        print_tricky_default_value(stream, "id");
        return stream;
    }
    const auto* named_type = dynamic_cast<const NamedTypeSymbol*>(&op.type);
    if (named_type) {
        switch (named_type->kind()) {
            case NamedTypeSymbol::Kind::TargetPrimitive: {
                auto name = named_type->name();
                if (name == "Bool") {
                    return stream << "false";
                }

                if (is_integer_type(name)) {
                    return stream << '0';
                }

                if (name == "Float32" || name == "Float64") {
                    return stream << "0.0";
                }

                if (name == "CFunc") {
                    return stream << emit_cangjie(op.type) << "(CPointer<Unit>())";
                }

                if (named_type->is_unit()) {
                    return stream << "()";
                }
                break;
            }
            case NamedTypeSymbol::Kind::TypeDef: {
                // Some time later it should be simplified to be just
                assert(dynamic_cast<const TypeAliasSymbol*>(named_type));
                    const auto* alias = static_cast<const TypeAliasSymbol*>(named_type);
                    const auto* named_target = dynamic_cast<const NamedTypeSymbol*>(alias->root_target());
                    if (named_target && named_target->is(NamedTypeSymbol::Kind::TargetPrimitive) &&
                        is_integer_type(named_target->name())) {
                        return stream << "unsafe{zeroValue<" << named_type->name() << ">()}";
                    }
                    const auto* target = alias->target();
                    assert(target);
                    return stream << default_value(*target);
                }
            case NamedTypeSymbol::Kind::Enum:
                return stream << '0';
            case NamedTypeSymbol::Kind::Interface:
            case NamedTypeSymbol::Kind::Protocol:
                print_tricky_default_value(stream, named_type->name());
                return stream;
            case NamedTypeSymbol::Kind::Struct:
                if (named_type->name() == "ObjCPointer") {
                    return stream << emit_cangjie(op.type) << "(CPointer<Unit>())";
                }
                break;
            default:
                break;
        }
    } else {
        const auto* varray = dynamic_cast<const VArraySymbol*>(&op.type);
        if (varray) {
            stream << '[';
            if (varray->size_) {
                auto value = default_value(*varray->element_type_);
                stream << value;
                for (size_t i = 1; i < varray->size_; ++i) {
                    stream << ", " << value;
                }
            }
            return stream << ']';
        }
    }
    return stream << emit_cangjie(op.type) << "()";
}

static void print_enum_constant_value(std::ostream& output, const NonTypeSymbol& symbol)
{
    assert(symbol.kind() == NonTypeSymbol::Kind::EnumConstant);
    auto value = symbol.enum_constant_value();
    auto* named_type = dynamic_cast<NamedTypeSymbol*>(symbol.return_type());
    if (named_type) {
        named_type = dynamic_cast<NamedTypeSymbol*>(&named_type->canonical_type());
        if (named_type && named_type->kind() == NamedTypeSymbol::Kind::TargetPrimitive) {
            auto type_name = named_type->name();
            // Avoid the "number exceeds the value range of type" Cangjie compiler error by
            // printing the value in a type-specific way.
            if (type_name == "Int8") {
                output << static_cast<int>(static_cast<int8_t>(value));
                return;
            }
            if (type_name == "Int16") {
                output << static_cast<int16_t>(value);
                return;
            }
            if (type_name == "Int32") {
                output << static_cast<int32_t>(value);
                return;
            }
            if (type_name == "Int64") {
                output << static_cast<int64_t>(value);
                return;
            }
            if (type_name == "UInt8") {
                output << static_cast<uint32_t>(static_cast<uint8_t>(value));
                return;
            }
            if (type_name == "UInt16") {
                output << static_cast<uint16_t>(value);
                return;
            }
            if (type_name == "UInt32") {
                output << static_cast<uint32_t>(value);
                return;
            }
            // "UInt64" is handled properly by fallback case.
        }
    }
    output << value;
}

static bool is_objc_compatible_parameters(NonTypeSymbol& method) noexcept
{
    for (const auto& parameter : method.parameters()) {
        if (!is_objc_compatible_parameter_type(*parameter.type())) {
            return false;
        }
    }
    return true;
}

namespace {
template <class Symbol> void write_result_type(std::ostream& output, const Symbol& symbol, const TypeLikeSymbol& type)
{
    output << ": ";
    if (!normal_mode() && symbol.is_nullable()) {
        output << '?';
    }
    output << emit_cangjie(type);
}
} // namespace

static void write_method_parameters(std::ostream& output, const NonTypeSymbol& method)
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
        write_result_type(output, parameter_symbol, *parameter_type);
        collect_import(*parameter_type);
    }
    output << ')';
}

static void write_foreign_name(std::ostream& output, const NonTypeSymbol& method)
{
    const std::string* selector;
    const auto& selector_attribute = method.selector_attribute();
    if (!selector_attribute.empty()) {
        selector = &selector_attribute;
    } else if (method.is_constructor() && method.name() != "init") {
        selector = &method.name();
    } else {
        return;
    }

    // FE supports `@ForeignName` in `@ObjCMirror` classes only.  In the
    // `GENERATE_DEFINITIONS` mode, `@ObjCMirror` is hidden, so hide `@ForeignName`
    // as well.
    auto hide_foreign_name = generate_definitions_mode();
    if (hide_foreign_name) {
        output << "/* ";
    }
    output << "@ForeignName[\"" << *selector << "\"]";
    if (hide_foreign_name) {
        output << " */";
    }
    output << ' ';
}

static const TypeLikeSymbol& erased_type(const TypeLikeSymbol& type) noexcept
{
    const auto* constructed = dynamic_cast<const ConstructedTypeSymbol*>(&type);
    if (constructed) {
        const auto* erased = constructed->original();
        if (erased) {
            return erased_type(*erased);
        }
    }
    return type;
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
            if (&erased_type(*member.parameter(i).type()) != &erased_type(*constructor.parameter(i).type())) {
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
        auto* base_type_decl = dynamic_cast<TypeDeclarationSymbol*>(&base_decl);
        if (base_type_decl) {
            for (auto& member : base_type_decl->members()) {
                if (member.is_member_method() && member.is_static() == is_static && member.selector() == selector) {
                    return &member;
                }
            }
            auto* overridden_method = get_overridden_method(*base_type_decl, prop);
            if (overridden_method) {
                return overridden_method;
            }
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
        auto* base_type_decl = dynamic_cast<TypeDeclarationSymbol*>(&base_decl);
        if (base_type_decl) {
            for (auto& member : base_type_decl->members()) {
                if (member.is_property() && member.is_static() == is_static && member.getter() == getter) {
                    return &member;
                }
            }
            auto* overridden_prop = get_overridden_property(*base_type_decl, getter, is_static);
            if (overridden_prop) {
                return overridden_prop;
            }
        }
    }
    return nullptr;
}

static void write_method(IndentingStringStream& output, bool is_interface, NonTypeSymbol& method)
{
    auto name = escape_keyword(method.name());
    auto* return_type = method.return_type();
    assert(return_type);
    auto hidden =
        normal_mode() && (!is_objc_compatible_parameter_type(*return_type) || !is_objc_compatible_parameters(method));
    if (hidden) {
        output.set_comment();
    }
    write_foreign_name(output, method);
    if (!is_interface) {
        output << "public ";
    }
    if (method.is_static()) {
        output << "static ";
    } else if (!is_interface) {
        output << "open ";
    }
    output << "func " << name;
    write_method_parameters(output, method);
    write_result_type(output, method, *return_type);
    if (generate_definitions_mode()) {
        if (return_type->is_unit()) {
            output << " { }";
        } else {
            output << " { " << default_value(*return_type) << " }";
        }
    }
    if (hidden) {
        output.reset_comment();
    } else {
        collect_import(*return_type);
    }
    output << '\n';
}

static bool is_hidden(const TypeDeclarationSymbol& decl, TypeLikeSymbol& type, const std::string& name)
{
    auto decl_kind = decl.kind();
    switch (decl_kind) {
        case NamedTypeSymbol::Kind::Interface:
        case NamedTypeSymbol::Kind::Protocol:
            // Current FE fails to process a field or property of an @ObjCMirror class if
            // the field and its type have the same name (no such problem in non-@ObjCMirror
            // classes). As a workaround, comment out such fields.
            if (name == type.name()) {
                return true;
            }
            break;
        default:
            break;
    }
    if (!normal_mode()) {
        // In experimental modes, all types are allowed.
        return false;
    }
    switch (decl_kind) {
        case NamedTypeSymbol::Kind::Interface:
        case NamedTypeSymbol::Kind::Protocol:
            // @ObjCMirror class/interface.  Only Objective-C compatible types can be used.
            return !is_objc_compatible_parameter_type(type);
        default:
            // This is a structure
            if (decl.is_ctype()) {
                // @C structure.  It could not be identified as @C if the type was non-@C.
                assert(type.is_ctype());

                // This is fully supported by C interoperability, never hide
                return false;
            }
            // Regular (non-@C) @ObjCMirror structures are not supported by the front end
            // yet, so in the NORMAL mode the @ObjCMirror attribute is commented out.  The
            // lack of the attribute means no restrictions to types being used.  But!  In
            // the EXPERIMENTAL mode it is @ObjCMirror, so in the EXPERIMENTAL mode only
            // Objective-C compatible types and CType are supported.  It is logical to
            // consider NORMAL as a subset of EXPERIMENTAL, so this restriction goes to
            // NORMAL as well.
            //
            // So, hide all but @C and Objective-C compatible.
            return !type.is_ctype() && !is_objc_compatible_parameter_type(type);
    }
}

void write_type_declaration(IndentingStringStream& output, TypeDeclarationSymbol* type)
{
    auto is_interface = type->is(NamedTypeSymbol::Kind::Protocol);
    const auto is_enum = type->is(NamedTypeSymbol::Kind::Enum);
    const auto is_struct_or_union = type->is(NamedTypeSymbol::Kind::Struct) || type->is(NamedTypeSymbol::Kind::Union);
    if (!is_enum) {
        if (type->is_ctype()) {
            output << "@C";
        } else {
            // The current FE allows applying `@ObjCMirror` to classes only, not to
            // structures/unions. In the `EXPERIMENTAL` mode, we mark structures/unions as
            // `@ObjCMirror` if they contain non-CType fields.  And in the
            // `GENERATE_DEFINITIONS` mode, we remove `@ObjCMirror` from both classes and
            // structures/unions.
            auto hide_objcmirror_attribute = (normal_mode() && is_struct_or_union) || generate_definitions_mode();
            if (hide_objcmirror_attribute) {
                output << "/* ";
            }
            output << "@ObjCMirror";
            if (hide_objcmirror_attribute) {
                output << " */";
            }
        }
        output << '\n';
    }
    output << "public ";
    if (is_interface) {
        // The current FE does not support interfaces at all.
        if (normal_mode()) {
            is_interface = false;
            output << "open class /*interface*/";
        } else {
            output << "interface";
        }
    } else if (is_struct_or_union) {
        output << "struct";
    } else if (is_enum) {
        output << "abstract sealed class";
    } else {
        output << "open class";
    }
    output << " " << escape_keyword(type->name());
    if (const auto parameter_count = type->parameter_count()) {
        output << "/*<";
        for (std::size_t i = 0; i < parameter_count; ++i) {
            if (i != 0) {
                output << ", ";
            }

            auto* parameter = type->parameter(i);
            assert(parameter);
            output << parameter->name();
        }
        output << ">*/";
    }
    const auto base_count = type->base_count();
    if (base_count) {
        auto hide_additional_bases = normal_mode();
        output << " <: ";
        auto* base = type->base(0);
        assert(base);
        output << emit_cangjie(base);
        collect_import(*base);
        if (base_count > 1) {
            if (hide_additional_bases) {
                output << " //";
            }
            for (std::size_t i = 1; i < base_count; ++i) {
                output << " & ";
                auto* base = type->base(i);
                assert(base);
                output << emit_cangjie(base);
                if (!hide_additional_bases) {
                    collect_import(*base);
                }
            }
            output << (hide_additional_bases ? "\n{\n" : " {\n");
        } else {
            output << " {\n";
        }
    } else {
        // TODO: Can @ObjCMirror classes and interfaces implement `id` implicitly?
        //  Discuss with FE developers.
        auto hidden = normal_mode();
        switch (type->kind()) {
            case NamedTypeSymbol::Kind::Interface:
            case NamedTypeSymbol::Kind::Protocol:
                // All @ObjCMirror classes and interfaces implement `id` (`ObjcId`) (unless in
                // normal mode, where interfaces are not supported yet)
                if (hidden) {
                    output << " /*";
                }
                output << " <: id";
                if (hidden) {
                    output << " */";
                }
                break;
            default:
                break;
        }
        output << " {\n";
    }
    output.indent();
    auto any_constructor_exists = false;
    auto default_constructor_exists = false;
    for (auto&& member : type->members()) {
        if (member.is_property()) {
            auto is_static = member.is_static();
            auto* getter = get_method_by_selector(*type, member.getter(), is_static);
            assert(getter);
            if (!get_overridden_method(*type, member) && !get_overridden_property(*type, member.getter(), is_static)) {
                auto* return_type = getter->return_type();
                assert(return_type);
                assert(!return_type->is_unit());
                const auto& name = getter->name();
                auto hidden = is_hidden(*type, *return_type, name);
                if (hidden) {
                    output.set_comment();
                }
                if (!is_interface) {
                    output << "public ";
                }
                if (is_static) {
                    output << "static ";
                } else if (!is_interface) {
                    output << "open ";
                }
                if (!member.is_readonly()) {
                    output << "mut ";
                }
                output << "prop " << escape_keyword(name);
                write_result_type(output, *getter, *return_type);
                if (generate_definitions_mode()) {
                    output << " {\n";
                    output.indent();
                    output << "get() { " << default_value(*return_type) << " }\n";
                    if (!member.is_readonly()) {
                        output << "set(v) { }\n";
                    }
                    output.dedent();
                    output << '}';
                }
                if (hidden) {
                    output.reset_comment();
                } else {
                    collect_import(*return_type);
                }
                output << '\n';
            }
        } else if (member.is_constructor()) {
            auto hidden = is_interface || (normal_mode() && !is_objc_compatible_parameters(member)) ||
                is_overloading_constructor(*type, member);
            if (hidden) {
                output.set_comment();
            } else {
                any_constructor_exists = true;
                if (!default_constructor_exists) {
                    default_constructor_exists = member.parameter_count() == 0;
                }
            }
            write_foreign_name(output, member);
            if (!is_interface) {
                output << "public ";
            }
            output << "init";
            write_method_parameters(output, member);
            if (generate_definitions_mode() && !is_interface) {
                output << " { }";
            }
            if (hidden) {
                output.reset_comment();
            }
            output << '\n';
        } else if (member.is_member_method()) {
            if (!get_property(*type, member) &&
                !get_overridden_property(*type, member.selector(), member.is_static())) {
                write_method(output, is_interface, member);
            }
        } else if (member.is_instance_variable()) {
            assert(member.is_instance());
            auto* return_type = member.return_type();
            assert(return_type);
            assert(!return_type->is_unit());
            assert(member.is_public() || member.is_protected());
            const auto& name = member.name();
            auto hidden = is_hidden(*type, *return_type, name);
            if (hidden) {
                output.set_comment();
            }
            output << (member.is_public() ? "public" : "protected") << " var " << escape_keyword(name);
            write_result_type(output, member, *return_type);
            if (generate_definitions_mode()) {
                output << " = " << default_value(*return_type);
            }
            if (hidden) {
                output.reset_comment();
            } else {
                collect_import(*return_type);
            }
            output << '\n';
        } else if (member.is_field()) {
            assert(member.is_instance());
            assert(is_struct_or_union || is_enum);
            auto* return_type = member.return_type();
            assert(return_type);
            assert(!return_type->is_unit());
            const auto& name = member.name();
            auto hidden = is_hidden(*type, *return_type, name);
            if (hidden) {
                output.set_comment();
            }
            output << "public var " << escape_keyword(name);
            write_result_type(output, member, *return_type);
            output << " = " << default_value(*return_type);
            if (hidden) {
                output.reset_comment();
            } else {
                collect_import(*return_type);
            }
            output << '\n';
        } else if (member.is_enum_constant()) {
            assert(is_enum);
            auto* return_type = member.return_type();
            assert(return_type);
            assert(!return_type->is_unit());
            output << "public static const " << escape_keyword(member.name());
            write_result_type(output, member, *return_type);
            output << " = ";
            collect_import(*return_type);
            print_enum_constant_value(output, member);
            output << '\n';
        } else {
            assert(false);
        }
    }

    // In the `GENERATE_DEFINITIONS` mode, add a fake default constructor if needed.
    // Otherwise, the following error can happen:
    // error: there is no non-parameter constructor in super class, please invoke
    // super call explicitly
    if (generate_definitions_mode() && any_constructor_exists && !default_constructor_exists) {
        output << "public init() { }";
    }

    output.dedent();
    output << "}" << std::endl;
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
                    write_type_declaration(output, type);
                } else {
                    assert(false);
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
                file_output << "import interoplib.objc.*\n\n";
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
