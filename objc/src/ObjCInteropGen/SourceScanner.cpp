// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "ClangSession.h"

#include <optional>
#include <stack>

#include <clang-c/Index.h>
#include <clang/AST/DeclObjC.h>
#include <clang/Basic/Version.h>

#include "FatalException.h"
#include "Logging.h"
#include "Strings.h"
#include "Universe.h"

[[nodiscard]] static bool operator==(const CXType& lhs, const CXType& rhs) noexcept
{
    return !!clang_equalTypes(lhs, rhs);
}

[[nodiscard]] static bool operator==(const CXCursor& lhs, const CXCursor& rhs) noexcept
{
    return !!clang_equalCursors(lhs, rhs);
}

namespace objcgen {

struct CXCursorHash {
    [[nodiscard]] size_t operator()(const CXCursor& x) const noexcept
    {
        return clang_hashCursor(x);
    }
};

class SourceScanner final : NonCopyable {
public:
    void visit(const CXCursor& cursor)
    {
        clang_visitChildren(cursor, visit, this);
    }

private:
    // See the comment in the 'get_owner_generic_type' method
    TypeDeclarationSymbol* last_interface_decl_ = nullptr;

    // Nesting stack.
    std::stack<NamedTypeSymbol*> current_;

    // We have to name the unnamed structs/unions/enums, use declaring file name +
    // incrementing index suffix
    std::unordered_map<CXCursor, NamedTypeSymbol*, CXCursorHash> unnamed_decls_;
    std::unordered_map<std::string, std::uint64_t> unnamed_decl_counts_;

    // Some symbols may be visited multiple times. Examples:
    // * The same symbol is processed multiple times because the corresponding
    //   header is shared between different translation units in one ClangSession.
    // * A method declaration can be repeated:
    //   - (void)foo;
    //   - (void)foo;
    // * A typedef can have multiple definitions:
    //   typedef int Int;
    //   typedef int32_t Int;
    // * A method or property declaration can be repeated in a category.
    // To avoid duplication, we will keep track visited symbols by their Unified
    // Symbol Resolution (USR).
    std::unordered_set<std::string> visited_symbols_;

    [[nodiscard]] NamedTypeSymbol* current_type() const noexcept
    {
        return current_.empty() ? nullptr : current_.top();
    }

    [[nodiscard]] TypeDeclarationSymbol& current_type_declaration() const noexcept
    {
        auto* named_type = current_type();
        assert(named_type);
        return named_type->as<TypeDeclarationSymbol>();
    }

    [[nodiscard]] std::size_t level() const noexcept
    {
        return current_.size();
    }

    [[nodiscard]] bool is_on_top_level() const noexcept
    {
        return current_.empty();
    }

    [[nodiscard]] TypeDeclarationSymbol& get_target_type_declaration();

    [[nodiscard]] std::vector<ParameterSymbol> get_function_parameters(const CXCursor& function_cursor);

    void add_top_level_function(const CXCursor& cursor);

    void add_property(std::string name, std::string getter, std::string setter, Modifiers modifiers);

    [[nodiscard]] Type get_method_result_type(
        TypeDeclarationSymbol& decl, const CXType& cx_result_type, Nullability nullability);

    [[nodiscard]] Type get_method_result_type(
        TypeDeclarationSymbol& decl, const CXCursor& method_cursor, Nullability nullability = Nullability::Unspecified);

    void add_member_method(const CXCursor& cursor, Modifiers modifiers);

    void add_constructor(const CXCursor& cursor);

    [[nodiscard]] std::string new_anonymous_name(const CXCursor& decl);

    template <CXTypeKind kind> [[nodiscard]] Type get_named_type(const CXType& type, Nullability nullability);

    [[nodiscard]] Type create_func_like_type(BuiltInTypeSymbol& func_like_symbol, const CXType& type);

    [[nodiscard]] Type type_like_symbol(const CXType& type, Nullability nullability = Nullability::Unspecified);

    void visit(const CXCursor& cursor, NamedTypeSymbol& symbol);

    [[nodiscard]] static CXChildVisitResult visit(CXCursor cursor, CXCursor parent, void* data)
    {
        static_cast<SourceScanner*>(data)->visit_impl(cursor, parent);
        return CXChildVisit_Continue;
    }

    // See the comment to 'visited_symbols_'.  This method return true if the symbol
    // under 'cursor' is considered to be fully processed and should not be visited
    // anymore.
    [[nodiscard]] bool is_fully_processed(const CXCursor& cursor);

    void visit_impl(const CXCursor& cursor, const CXCursor& parent);

    /**
     * Get the generic type declaration the currently processed type parameter
     * belongs to.
     */
    [[nodiscard]] TypeDeclarationSymbol& get_owner_generic_type() const noexcept;

    [[nodiscard]] Type get_type_parameter(const CXType& type, Nullability nullability) const;
};

class ClangSessionImpl final : public ClangSession, private NonCopyable {
    CXIndex index_;
    SourceScanner scanner_;

public:
    ClangSessionImpl()
    {
        index_ = clang_createIndex(0, 1);
    }

    ~ClangSessionImpl() override;

    [[nodiscard]] CXIndex index() const
    {
        return index_;
    }

    [[nodiscard]] SourceScanner& scanner()
    {
        return scanner_;
    }

private:
    void parse_sources(const std::vector<std::string>& files, const std::vector<std::string>& arguments) override;
};

std::unique_ptr<ClangSession> ClangSession::create()
{
    return std::make_unique<ClangSessionImpl>();
}

ClangSessionImpl::~ClangSessionImpl()
{
    clang_disposeIndex(index());
}

class String {
public:
    explicit String(CXString string) noexcept : string_(string)
    {
    }

    ~String()
    {
        clang_disposeString(string_);
    }

    [[nodiscard]] bool empty() const noexcept
    {
        const auto* data = c_str();
        return !data || !*data;
    }

    [[nodiscard]] const char* c_str() const noexcept
    {
        return clang_getCString(string_);
    }

    [[nodiscard]] std::string string() const
    {
        return clang_getCString(string_);
    }

    [[nodiscard]] std::string_view string_view() const noexcept
    {
        return clang_getCString(string_);
    }

private:
    CXString string_;
};

static std::ostream& operator<<(std::ostream& stream, const String& string)
{
    return stream << string.string_view();
}

[[nodiscard]] static std::string as_string(CXString string)
{
    return String(string).string();
}

[[nodiscard]] static bool is_valid(const CXCursor& cursor)
{
    return !clang_isInvalid(cursor.kind);
}

[[nodiscard]] static bool is_valid(const CXType& type)
{
    return type.kind != CXType_Invalid;
}

[[nodiscard]] static bool is_canonical(const CXCursor& cursor)
{
    assert(is_valid(cursor));
    const auto canonical = clang_getCanonicalCursor(cursor);
    assert(is_valid(canonical));
    return cursor == canonical;
}

[[nodiscard]] static bool is_defining(const CXCursor& cursor)
{
    assert(is_valid(cursor));
    const auto definition = clang_getCursorDefinition(cursor);
    if (!is_valid(definition))
        return true;
    return cursor == definition;
}

[[nodiscard]] static bool is_defining(const CXType& type, const CXCursor& cursor)
{
    assert(is_valid(type));
    assert(is_valid(cursor));
    const auto declaration = clang_getTypeDeclaration(type);
    assert(is_valid(declaration));
    return cursor == declaration;
}

[[nodiscard]] static bool is_null_location(const CXSourceLocation& loc)
{
    return clang_equalLocations(loc, clang_getNullLocation());
}

[[nodiscard]] static Location get_location(const CXCursor& decl)
{
    assert(is_valid(decl));
    auto loc = clang_getCursorLocation(decl);
    if (is_null_location(loc)) {
        return {};
    }
    Location location;
    CXFile file;
    clang_getFileLocation(loc, &file, &location.pos_.line_, &location.pos_.col_, nullptr);
    if (!file) {
        return {};
    }
    location.file_ = as_string(clang_getFileName(file));
    if (!location.file_.is_absolute()) {
        location.file_ = std::filesystem::absolute(location.file_);
    }
    return location;
}

[[nodiscard]] static std::string declaring_file_name(const CXCursor& decl)
{
    assert(is_valid(decl));
    auto location = get_location(decl);
    assert(location.file_.has_stem());
    return location.file_.stem().u8string();
}

static void set_definition_location(const CXCursor& decl, FileLevelSymbol& symbol)
{
    assert(is_valid(decl));
    auto loc = get_location(decl);
    if (!loc.is_null()) {
        symbol.set_definition_location(loc);
    }
}

template <class Decl>
[[nodiscard]] const std::enable_if_t<std::is_base_of_v<clang::Decl, Decl>, Decl>& cursor_to_decl(const CXCursor& cursor)
{
    const auto* decl = llvm::dyn_cast_or_null<Decl>(static_cast<const clang::Decl*>(cursor.data[0]));
    assert(decl);
    return *decl;
}

[[nodiscard]] static clang::QualType type_to_qual_type(const CXType& type)
{
    return clang::QualType::getFromOpaquePtr(type.data[0]);
}

Type SourceScanner::create_func_like_type(BuiltInTypeSymbol& func_like_symbol, const CXType& type)
{
    assert(type.kind == CXType_FunctionProto || type.kind == CXType_FunctionNoProto);
    int num_arg_types = clang_getNumArgTypes(type);
    assert(num_arg_types >= 0);
    std::vector<Type> parameters;
    parameters.reserve(static_cast<size_t>(num_arg_types) + 1);
    parameters.emplace_back(type_like_symbol(clang_getResultType(type)));
    for (int i = 0; i < num_arg_types; ++i) {
        parameters.emplace_back(type_like_symbol(clang_getArgType(type, static_cast<unsigned>(i))));
    }
    return Type(func_like_symbol, std::move(parameters));
}

std::string SourceScanner::new_anonymous_name(const CXCursor& decl)
{
    assert(clang_Cursor_isAnonymous(decl));
    auto file_name = declaring_file_name(decl);
    std::uint64_t index = 1;
    if (auto&& [item, inserted] = unnamed_decl_counts_.try_emplace(file_name, index); !inserted) {
        index = ++item->second;
    }
    return "__" + file_name + '_' + std::to_string(index);
}

[[nodiscard]] static Nullability get_nullability(const CXType& type)
{
    switch (clang_Type_getNullability(type)) {
        case CXTypeNullability_NonNull:
            return Nullability::Nonnull;
        case CXTypeNullability_Nullable:
#if CLANG_VERSION_MAJOR >= 12
        case CXTypeNullability_NullableResult:
#endif
            return Nullability::Nullable;
        default:
            return Nullability::Unspecified;
    }
}

[[nodiscard]] static UnexposedTypeSymbol& create_unexposed_type_symbol(const CXType& type, std::string name)
{
    auto size = clang_Type_getSizeOf(type);
    return *new UnexposedTypeSymbol(std::move(name), size < 0 ? 0 : static_cast<size_t>(size));
}

template <CXTypeKind type_kind> Type SourceScanner::get_named_type(const CXType& type, Nullability nullability)
{
    static_assert(type_kind == CXType_Unexposed || type_kind == CXType_Record || type_kind == CXType_Enum ||
        type_kind == CXType_Typedef || type_kind == CXType_ObjCInterface);
    auto decl = clang_getTypeDeclaration(type);
    if constexpr (type_kind != CXType_Unexposed) {
        assert(type.kind == type_kind);
        assert(is_valid(decl));
    }

    bool unnamed;
    if constexpr (type_kind == CXType_Record || type_kind == CXType_Enum) {
        if constexpr (type_kind == CXType_Record) {
            if (clang_Cursor_isAnonymousRecordDecl(decl)) {
                // This is an anonymous struct or union, like this:
                //
                // struct T {
                //     struct {
                //         int x;
                //     };
                // };
                //
                // Its members are considered to be members of the enclosing struct or union.  The structure itself is
                // ignored and does not go to Cangjie.
                return {};
            }
        }

        unnamed = clang_Cursor_isAnonymous(decl);
        if (unnamed) {
            // This is a struct/union/enum without a tag (but not an anonymous struct/union).
            auto it = unnamed_decls_.find(decl);
            if (it != unnamed_decls_.end()) {
                return Type(*it->second, nullability);
            }
        }
    } else {
        unnamed = false;
    }

    NamedTypeSymbol::Kind symbol_kind;
    if constexpr (type_kind == CXType_Typedef) {
        symbol_kind = NamedTypeSymbol::Kind::TypeDef;
    } else if constexpr (type_kind == CXType_Unexposed) {
        symbol_kind = NamedTypeSymbol::Kind::Unexposed;
    } else if constexpr (type_kind == CXType_Enum) {
        symbol_kind = NamedTypeSymbol::Kind::Enum;
    } else if constexpr (type_kind == CXType_ObjCInterface) {
        symbol_kind =
            decl.kind == CXCursor_ObjCProtocolDecl ? NamedTypeSymbol::Kind::Protocol : NamedTypeSymbol::Kind::Interface;
    } else {
        static_assert(type_kind == CXType_Record);
        symbol_kind = decl.kind == CXCursor_UnionDecl ? NamedTypeSymbol::Kind::Union : NamedTypeSymbol::Kind::Struct;
    }

    std::string name;
    if constexpr (type_kind == CXType_Unexposed) {
        name = as_string(is_valid(decl) ? clang_getCursorSpelling(decl) : clang_getTypeSpelling(type));
    } else if constexpr (type_kind == CXType_Record || type_kind == CXType_Enum) {
        if (unnamed) {
            name = new_anonymous_name(decl);
        } else {
            name = as_string(clang_getCursorSpelling(decl));
            if (name.empty()) {
                // This must be an unnamed (but not anonymous) struct/union/enum declared inside
                // a typedef declaration, like this:
                //     typedef struct {} A;
                // In such a case clang_getTypeSpelling applied to the type of this unnamed
                // cursor returns the name of the typedef (sic!).  Do not ask why but that is
                // very convenient for us.
                name = as_string(clang_getTypeSpelling(type));
            }
        }
    } else {
        name = as_string(clang_getCursorSpelling(decl));
    }
    assert(!name.empty());

    auto& universe = Universe::get();
    auto* symbol = universe.type(symbol_kind, name);
    if (!symbol) {
        if constexpr (type_kind == CXType_Typedef) {
            auto cx_target = clang_getTypedefDeclUnderlyingType(decl);
            assert(is_valid(cx_target));
            auto target = type_like_symbol(cx_target);
            const auto& target_symbol = target.symbol();
            if (target.is_optionable_reference()) {
                if (target.nullability() == Nullability::Unspecified) {
                    target.set_nullability(Nullability::Nonnull);
                }
            } else if (const auto* target_as_alias = dynamic_cast<const TypeAliasSymbol*>(&target_symbol);
                target_as_alias && target.nullability() == Nullability::Nullable &&
                target_as_alias->target().nullability() == Nullability::Nullable) {
                target.set_nullability(Nullability::Nonnull);
            }
            const auto* target_as_named = dynamic_cast<const NamedTypeSymbol*>(&target_symbol);
            if (target_as_named && target_as_named->name() == name) {
                // This can be one of the following:
                //
                // typedef id<MyProtocol> MyProtocol;
                // typedef struct MyStruct { ... } MyStruct;
                // typedef int Int32;
                //
                // Do not create such typedef at all.  Use directly its target everywhere.
                return target;
            }
            symbol = new TypeAliasSymbol(std::move(name), std::move(target));
            auto loc = get_location(decl);
            if (!loc.is_null()) {
                symbol->set_definition_location(loc);
            }
        } else if constexpr (type_kind == CXType_Unexposed) {
            symbol = &create_unexposed_type_symbol(type, std::move(name));
        } else {
            auto loc = get_location(decl);
            if (loc.is_null()) {
                if constexpr (type_kind == CXType_ObjCInterface) {
                    // The only class without a declaration in a source file must be the built-in
                    // class Protocol.  Currently it is not supported by interop and has no
                    // declaration at the Cangjie side either.  However, we create the corresponding
                    // symbol.  References to it will be commented out in the normal mode (but not
                    // in experimental).
                    if (name == "Protocol") {
                        symbol = new TypeDeclarationSymbol(symbol_kind, std::move(name));
                    } else {
                        // A built-in declaration that has no file location.  Represent it as unexposed.
                        symbol = &create_unexposed_type_symbol(type, std::move(name));
                    }
                } else {
                    // A built-in declaration that has no file location.  Represent it as unexposed.
                    symbol = &create_unexposed_type_symbol(type, std::move(name));
                }
            } else {
                if constexpr (type_kind == CXType_Enum) {
                    auto underlying_type = clang_getEnumDeclIntegerType(decl);
                    assert(is_valid(underlying_type));
                    symbol = new EnumDeclarationSymbol(
                        std::move(name), type_like_symbol(underlying_type).symbol().as<NamedTypeSymbol>());
                } else {
                    symbol = new TypeDeclarationSymbol(symbol_kind, std::move(name));
                }
                if constexpr (type_kind == CXType_Record || type_kind == CXType_Enum) {
                    if (unnamed) {
                        assert(unnamed_decls_.find(decl) == unnamed_decls_.end());
                        unnamed_decls_.try_emplace(decl, symbol);
                    }
                }
                symbol->set_definition_location(loc);
            }
        }

        universe.register_type(*symbol);
    }

    return Type(*symbol, nullability);
}

[[nodiscard]] static TypeDeclarationSymbol& get_type_declaration(const CXCursor& cursor, NamedTypeSymbol::Kind kind)
{
    auto& universe = Universe::get();
    String name(clang_getCursorSpelling(cursor));
    auto* result = universe.type(kind, name.string_view());
    if (result) {
        return result->as<TypeDeclarationSymbol>();
    }
    auto& new_result = *new TypeDeclarationSymbol(kind, name.string());
    universe.register_type(new_result);
    set_definition_location(cursor, new_result);
    return new_result;
}

[[nodiscard]] static TypeDeclarationSymbol& protocol_symbol(const CXType& objc_object_type, unsigned i)
{
    assert(objc_object_type.kind == CXType_ObjCObject);
    assert(i < clang_Type_getNumObjCProtocolRefs(objc_object_type));
    auto protocol_decl = clang_Type_getObjCProtocolDecl(objc_object_type, i);
    assert(protocol_decl.kind == CXCursor_ObjCProtocolDecl);
    return get_type_declaration(protocol_decl, NamedTypeSymbol::Kind::Protocol);
}

struct UndecorateResult {
    std::string_view undecorated_type_name;
    std::vector<Type> protocols;
};

[[nodiscard]] static TypeDeclarationSymbol& get_protocol_by_name(std::string_view name)
{
    auto& universe = Universe::get();
    auto* protocol = universe.type(NamedTypeSymbol::Kind::Protocol, std::string(name));
    return protocol ? protocol->as<TypeDeclarationSymbol>() : universe.id();
}

/**
 * The type parameter name can be specified with a narrowing protocol.  Like in
 * this sample (`T<NSCopying>` instead of just `T`):
 * <pre>
 *     @interface A<T> : NSObject
 *     - (void) foo: (T <NSCopying>) x;
 *     @end
 * </pre>
 *
 * Also the type parameter name can be prefixed with the `const`,
 * `__unsafe_unretained`, or `__strong` modifier.
 *
 * <p> We need a pure name without any "decorations", to make it possible to
 * find the parameter in its owner's parameter list.  The pure name hardly can
 * be obtained with the libclang API.  Let us "undecorate" it by ourselves.
 */
[[nodiscard]] static UndecorateResult undecorate_parameter_type_name(std::string_view decorated_type_name)
{
    auto without_prefix =
        remove_prefix(remove_prefix(remove_prefix(decorated_type_name, "__unsafe_unretained "), "__strong "), "const ");
    auto opening_bracket = without_prefix.find('<');
    if (opening_bracket == std::string_view::npos || without_prefix.back() != '>') {
        return {without_prefix, {}};
    }
    auto protocol_names = without_prefix.substr(opening_bracket + 1, without_prefix.size() - opening_bracket - 2);
    std::vector<Type> protocols;
    for (;;) {
        auto comma = protocol_names.find(',');
        if (comma == std::string_view::npos) {
            protocols.emplace_back(get_protocol_by_name(protocol_names));
            break;
        }
        protocols.emplace_back(get_protocol_by_name(protocol_names.substr(0, comma)));
        protocol_names = protocol_names.substr(comma + 1);
    }
    return {without_prefix.substr(0, opening_bracket), protocols};
}

[[nodiscard]] static PrimitiveTypeSymbol* primitive_type(const CXType& type)
{
    assert(type.kind >= CXType_FirstBuiltin && type.kind <= CXType_LastBuiltin && type.kind != CXType_NullPtr &&
        type.kind != CXType_Overload && type.kind != CXType_Dependent && type.kind != CXType_ObjCId &&
        type.kind != CXType_ObjCClass && type.kind != CXType_ObjCSel);
    auto& universe = Universe::get();
    switch (type.kind) {
        case CXType_Void:
            return &universe.unit();
        case CXType_Bool:
            // For binary compatibility with Cangjie's `Bool`, we do not support platforms
            // where `sizeof(_Bool) != 1`
            assert(clang_Type_getSizeOf(type) == 1);

            return &universe.boolean();
        default:
            break;
    }
    auto size = clang_Type_getSizeOf(type);
    auto cpp = type_to_qual_type(type);
    if (cpp->isSignedIntegerOrEnumerationType()) {
        switch (static_cast<PrimitiveSize>(size)) {
            case PrimitiveSize::One:
                return &universe.int8();
            case PrimitiveSize::Two:
                return &universe.int16();
            case PrimitiveSize::Four:
                return &universe.int32();
            case PrimitiveSize::Eight:
                return &universe.int64();
            default:
                break;
        }
    } else if (cpp->isUnsignedIntegerOrEnumerationType()) {
        switch (static_cast<PrimitiveSize>(size)) {
            case PrimitiveSize::One:
                return &universe.uint8();
            case PrimitiveSize::Two:
                return &universe.uint16();
            case PrimitiveSize::Four:
                return &universe.uint32();
            case PrimitiveSize::Eight:
                return &universe.uint64();
            default:
                break;
        }
    } else if (cpp->isFloatingType()) {
        switch (static_cast<PrimitiveSize>(size)) {
            case PrimitiveSize::Two:
                return &universe.float16();
            case PrimitiveSize::Four:
                return &universe.float32();
            case PrimitiveSize::Eight:
                return &universe.float64();
            default:
                break;
        }
    }
    return size <= 0 ? &universe.unit() : nullptr;
}

class CategoryDeclarationSymbol final : public TypeDeclarationSymbol {
public:
    explicit CategoryDeclarationSymbol(std::string name, TypeDeclarationSymbol& interface) noexcept
        : TypeDeclarationSymbol(Kind::Category, std::move(name)), interface_(&interface)
    {
    }

    [[nodiscard]] TypeDeclarationSymbol& interface() const noexcept
    {
        return *interface_;
    }

private:
    TypeDeclarationSymbol* const interface_;
};

TypeDeclarationSymbol& SourceScanner::get_owner_generic_type() const noexcept
{
    // Type declarations nested in ObjC interfaces are visited NOT as children of
    // those interfaces, but as top-level types (not having parents formally).  But
    // they can reference type parameters of their actual parents!
    //
    // For example:
    //     @interface M <__covariant ElementT>
    //     typedef ElementT E;
    //     @end
    //
    // In this example the declaration of the type ElementT will be visited as a
    // child of the top-level E declaration, but we need the information about its
    // real generic Objective-C owner which is M in this case.  Luckily, such nested
    // non-ObjC declarations are visited right after processing their real owners.
    // So that we can track the previous ObjC declaration and use it for type
    // parameter lookup here.
    //
    // It seems macOS/iOS system headers do not declare nested types, but they are
    // usual in GNUstep.  For example the GSSetEnumeratorBlock typedef, which is
    // defined in the middle of the NSSet<ElementT> definition and references the
    // ElementT type parameter.
    if (!is_on_top_level()) {
        auto& owner_type = current_type_declaration();
        switch (owner_type.kind()) {
            case NamedTypeSymbol::Kind::Interface:
            case NamedTypeSymbol::Kind::Category:
                return owner_type;
            default:
                break;
        }
    }
    assert(last_interface_decl_);
    assert(last_interface_decl_->is(NamedTypeSymbol::Kind::Interface) ||
        last_interface_decl_->is(NamedTypeSymbol::Kind::Category));
    return *last_interface_decl_;
}

Type SourceScanner::get_type_parameter(const CXType& type, Nullability nullability) const
{
    String decorated_type_name(clang_getTypeSpelling(type));
    auto [undecorated_type_name, protocols] = undecorate_parameter_type_name(decorated_type_name.string_view());
    auto& owner_type = get_owner_generic_type();
    const auto parameter_count = owner_type.parameter_count();
    for (std::size_t i = 0; i < parameter_count; ++i) {
        auto* parameter = &owner_type.parameter(i);
        if (parameter->name() == undecorated_type_name) {
            if (owner_type.kind() == NamedTypeSymbol::Kind::Category) {
                auto& interface = owner_type.as<CategoryDeclarationSymbol>().interface();
                assert(parameter_count == interface.parameter_count());
                parameter = &interface.parameter(i);
            }
            return Type(*parameter, std::move(protocols), nullability);
        }
    }
    assert(false && "Unknown type parameter");
    return {};
}

Type SourceScanner::type_like_symbol(const CXType& type, Nullability nullability)
{
    assert(type.kind != CXType_Invalid);

    // Libclang prior version 16 does not expose the `clang_getUnqualifiedType`
    // function.  For older clang, the type name will be cleared from
    // const\volatile\restrict manually in the end of this function;
#if CLANG_VERSION_MAJOR >= 16
    if (clang_isConstQualifiedType(type) || clang_isVolatileQualifiedType(type) ||
        clang_isRestrictQualifiedType(type)) {
        return type_like_symbol(clang_getUnqualifiedType(type), nullability);
    }
#endif

    switch (type.kind) {
        case CXType_ObjCObject: {
            auto baseCXType = clang_Type_getObjCObjectBaseType(type);
            if (baseCXType.kind == CXType_ObjCId) {
                // This is `id` qualified with a list of protocols (`id<Protocol1, Protocol2>`)
                auto& id_type = Universe::get().id();
                auto num_protocols = clang_Type_getNumObjCProtocolRefs(type);
                assert(num_protocols);
                if (num_protocols == 1) {
                    // In Cangjie, `id` qualified with just one protocol can be represented as a
                    // reference-to-interface.
                    return Type(protocol_symbol(type, 0), nullability);
                }
                std::vector<Type> protocols;
                protocols.reserve(num_protocols);
                for (decltype(num_protocols) i = 0; i < num_protocols; ++i) {
                    protocols.emplace_back(protocol_symbol(type, i), Nullability::Nonnull);
                }
                return Type(id_type, std::move(protocols), nullability);
            }

            // This is a generic class qualified with a list of type arguments
            // (`NSArray<SomeClass>`)
            auto base_type = type_like_symbol(baseCXType, nullability);
            auto type_arg_count = clang_Type_getNumObjCTypeArgs(type);
            std::vector<Type> type_args;
            type_args.reserve(type_arg_count);
            for (decltype(type_arg_count) i = 0; i < type_arg_count; ++i) {
                auto& type_arg = type_args.emplace_back(type_like_symbol(clang_Type_getObjCTypeArg(type, i)));
                switch (type_arg.kind()) {
                    case Type::Kind::Named:
                    case Type::Kind::TypeParam:
                        // Type arguments cannot have explicitly specified nullability.
                        type_arg.set_nullability(Nullability::Nonnull);
                        break;
                    default:
                        break;
                }
            }
            base_type.set_parameters(std::move(type_args));
            return base_type;
        }

        case CXType_ObjCObjectPointer:
            return type_like_symbol(clang_getPointeeType(type), nullability);

        case CXType_Pointer: {
            auto pointee = type_like_symbol(clang_getPointeeType(type));
            return pointee.kind() == Type::Kind::Function ? pointee : Type(Universe::get().pointer(), {pointee});
        }

        case CXType_BlockPointer:
            return create_func_like_type(Universe::get().block(), clang_getPointeeType(type));

        case CXType_Elaborated:
            return type_like_symbol(clang_Type_getNamedType(type), nullability);

        case CXType_Unexposed: {
            // Libclang bug?  When CXTranslationUnit_IncludeAttributedTypes is specified,
            // the type kind of some objects is unexpectedly and incorrectly reported as
            // CXType_Unexposed rather than CXType_Attributed.  Try to call
            // clang_Type_getModifiedType.  If it returns a valid type, assume it is
            // actually CXType_Attributed.
            auto modified_type = clang_Type_getModifiedType(type);
            return modified_type.kind == CXType_Invalid ? get_named_type<CXType_Unexposed>(type, nullability)
                                                        : type_like_symbol(modified_type, get_nullability(type));
        }

        case CXType_Attributed: {
            auto modified_type = clang_Type_getModifiedType(type);
            assert(modified_type.kind != CXType_Invalid);
            return type_like_symbol(modified_type, get_nullability(type));
        }

        case CXType_ObjCTypeParam:
            return get_type_parameter(type, nullability);

        case CXType_FunctionProto:
        case CXType_FunctionNoProto:
            return create_func_like_type(Universe::get().func(), type);

        case CXType_IncompleteArray:
            return Type(type_like_symbol(clang_getArrayElementType(type)), 0);

        case CXType_ConstantArray:
            return Type(
                type_like_symbol(clang_getArrayElementType(type)), static_cast<size_t>(clang_getArraySize(type)));

#if CLANG_VERSION_MAJOR >= 11
        case CXType_Atomic: {
            auto value_type = clang_Type_getValueType(type);
            assert(is_valid(value_type));
            return type_like_symbol(value_type);
        }
#endif

        case CXType_ObjCId:
            return Type(Universe::get().id(), nullability);

        case CXType_ObjCClass:
            return Type(Universe::get().clazz(), nullability);

        case CXType_ObjCSel:
            return Type(Universe::get().sel(), nullability);

        case CXType_ObjCInterface:
            assert(clang_getCanonicalType(type) == type);
            return get_named_type<CXType_ObjCInterface>(type, nullability);

        case CXType_Typedef:
            // It makes sense to call clang_getCanonicalType(type) if needed
            return get_named_type<CXType_Typedef>(type, nullability);

        case CXType_Record:
            return get_named_type<CXType_Record>(type, nullability);

        case CXType_Enum:
            return get_named_type<CXType_Enum>(type, nullability);

        default: {
            auto* primitive_type_symbol = primitive_type(type);
            return primitive_type_symbol ? Type(*primitive_type_symbol)
                                         : get_named_type<CXType_Unexposed>(type, nullability);
        }
    }
}

TypeDeclarationSymbol& SourceScanner::get_target_type_declaration()
{
    auto& decl = current_type_declaration();
    return decl.is(NamedTypeSymbol::Kind::Category) ? decl.as<CategoryDeclarationSymbol>().interface() : decl;
}

void SourceScanner::add_property(std::string name, std::string getter, std::string setter, Modifiers modifiers)
{
    assert(!is_on_top_level());
    get_target_type_declaration().add_property(std::move(name), std::move(getter), std::move(setter), modifiers);
}

[[nodiscard]] static bool is_init_method(const CXCursor& cursor) noexcept
{
    assert(cursor.kind == CXCursor_ObjCInstanceMethodDecl);
    return cursor_to_decl<clang::ObjCMethodDecl>(cursor).getMethodFamily() == clang::ObjCMethodFamily::OMF_init;
}

std::vector<ParameterSymbol> SourceScanner::get_function_parameters(const CXCursor& function_cursor)
{
    auto num_args = clang_Cursor_getNumArguments(function_cursor);
    assert(num_args >= 0);
    auto n = static_cast<unsigned>(num_args);
    std::vector<ParameterSymbol> parameters;
    parameters.reserve(n);
    for (unsigned i = 0; i < n; ++i) {
        auto param_cursor = clang_Cursor_getArgument(function_cursor, i);
        auto name = as_string(clang_getCursorSpelling(param_cursor));
        if (name.empty()) {
            // Objective-C function parameters can be nameless.  Synthesize a name (needed
            // in Cangjie).
            name = n == 1 ? "x" : 'x' + std::to_string(i + 1);
        }
        parameters.emplace_back(std::move(name), type_like_symbol(clang_getCursorType(param_cursor)));
    }
    return parameters;
}

void SourceScanner::add_top_level_function(const CXCursor& cursor)
{
    assert(is_on_top_level());
    set_definition_location(cursor,
        Universe::get().register_top_level_function(as_string(clang_getCursorSpelling(cursor)),
            type_like_symbol(clang_getCursorResultType(cursor)), get_function_parameters(cursor),
            clang_getCursorLinkage(cursor) == CXLinkage_Internal ? ModifierInternalLinkage : 0));
}

Type SourceScanner::get_method_result_type(
    TypeDeclarationSymbol& decl, const CXType& cx_result_type, Nullability nullability)
{
    switch (cx_result_type.kind) {
        // Libclang bug? When CXTranslationUnit_IncludeAttributedTypes is specified, the
        // type kind of some objects is unexpectedly and incorrectly reported as
        // CXType_Unexposed rather than CXType_Attributed.
        case CXType_Unexposed: {
            auto modified_cx_result_type = clang_Type_getModifiedType(cx_result_type);
            if (modified_cx_result_type.kind != CXType_Invalid) {
                return get_method_result_type(decl, modified_cx_result_type, get_nullability(cx_result_type));
            }
            break;
        }

        case CXType_Attributed: {
            auto modified_cx_result_type = clang_Type_getModifiedType(cx_result_type);
            assert(modified_cx_result_type.kind != CXType_Invalid);
            return get_method_result_type(decl, modified_cx_result_type, get_nullability(cx_result_type));
        }
        default:
            break;
    }
    return type_like_symbol(cx_result_type, nullability);
}

Type SourceScanner::get_method_result_type(
    TypeDeclarationSymbol& decl, const CXCursor& method_cursor, Nullability nullability)
{
    return get_method_result_type(decl, clang_getCursorResultType(method_cursor), nullability);
}

void SourceScanner::add_member_method(const CXCursor& cursor, Modifiers modifiers)
{
    assert(!is_on_top_level());
    auto& decl = get_target_type_declaration();
    decl.add_member_method(as_string(clang_getCursorSpelling(cursor)), get_method_result_type(decl, cursor),
        get_function_parameters(cursor), modifiers);
}

void SourceScanner::add_constructor(const CXCursor& cursor)
{
    assert(!is_on_top_level());
    auto& decl = get_target_type_declaration();
    decl.add_constructor(as_string(clang_getCursorSpelling(cursor)), get_method_result_type(decl, cursor),
        get_function_parameters(cursor));
}

[[nodiscard]] static std::array<uint64_t, 2> get_enum_constant_value(const CXCursor& cursor)
{
    static_assert(llvm::APInt::APINT_WORD_SIZE == sizeof(uint64_t));
    auto val = cursor_to_decl<clang::EnumConstantDecl>(cursor).getInitVal();
    assert(val.getNumWords() <= 2);
    const auto* raw_value = val.getRawData();
    return {raw_value[0], val.getBitWidth() <= llvm::APInt::APINT_BITS_PER_WORD ? 0 : raw_value[1]};
}

void SourceScanner::visit(const CXCursor& cursor, NamedTypeSymbol& symbol)
{
    current_.push(&symbol);
    switch (symbol.kind()) {
        case NamedTypeSymbol::Kind::Interface:
        case NamedTypeSymbol::Kind::Category:
            last_interface_decl_ = &symbol.as<TypeDeclarationSymbol>();
            break;
        default:
            break;
    }
    visit(cursor);
    current_.pop();
}

bool SourceScanner::is_fully_processed(const CXCursor& cursor)
{
    switch (clang_getCursorKind(cursor)) {
        case CXCursor_StructDecl:
        case CXCursor_UnionDecl:
        case CXCursor_EnumDecl: {
            // There can be multiple cursors of this kind for this USR.  But only after
            // processing the definition cursor the symbol is considered fully processed.
            auto cursor_usr = as_string(clang_getCursorUSR(cursor));
            if (!cursor_usr.empty()) {
                return clang_isCursorDefinition(cursor) ? !visited_symbols_.emplace(std::move(cursor_usr)).second
                                                        : visited_symbols_.find(cursor_usr) != visited_symbols_.end();
            }
            break;
        }
        case CXCursor_FunctionDecl:
        case CXCursor_ObjCInterfaceDecl:
        case CXCursor_ObjCCategoryDecl:
        case CXCursor_ObjCProtocolDecl:
        case CXCursor_ObjCPropertyDecl:
        case CXCursor_ObjCInstanceMethodDecl:
        case CXCursor_ObjCClassMethodDecl:
        case CXCursor_TypedefDecl: {
            // For nterfaces, categories, and protocols, there can be only one cursor of
            // this kind and USR.  For functions, properties, and typedefs, there can be
            // multiple cursors.  In both cases, the symbol is considered fully processed
            // after the first processed cursor.
            String cursor_usr(clang_getCursorUSR(cursor));
            if (!cursor_usr.empty()) {
                return !visited_symbols_.emplace(cursor_usr.string()).second;
            }
            break;
        }
        default:
            break;
    }
    return false;
}

void SourceScanner::visit_impl(const CXCursor& cursor, const CXCursor& parent)
{
    assert(is_valid(cursor));
    assert(is_valid(parent));

    const auto cursor_kind = clang_getCursorKind(cursor);

    auto fully_processed = is_fully_processed(cursor);

    if (verbosity >= LogLevel::DEBUG) {
        const auto level = this->level();
        for (std::size_t i = 0; i < level; i++) {
            std::cout << ' ';
        }

        std::cout << String(clang_getCursorKindSpelling(cursor_kind)) << ' ' << String(clang_getCursorSpelling(cursor));

        auto type = clang_getCursorType(cursor);
        if (type.kind != CXType_Invalid) {
            std::cout << " <" << String(clang_getTypeSpelling(type)) << '>';
        }

        if (clang_Cursor_isAnonymousRecordDecl(cursor)) {
            std::cout << " [anonymous]";
        }

        if (clang_Cursor_isAnonymous(cursor)) {
            std::cout << " [unnamed]";
        }

        if (fully_processed) {
            std::cout << " [visited]";
        }

        std::cout << std::endl;
    }

    if (fully_processed) {
        return;
    }

    // Ignore declarations with the `unavailable` attribute. For example:
    //
    // NS_AUTOMATED_REFCOUNT_UNAVAILABLE
    // @interface NSAutoreleasePool : NSObject {
    //
    // where the macro `NS_AUTOMATED_REFCOUNT_UNAVAILABLE` is defined as
    //
    // #define NS_AUTOMATED_REFCOUNT_UNAVAILABLE
    //     __attribute__((unavailable("not available in automatic reference counting mode")))
    //
    // It would make sense to take into account particular platform.
    int always_unavailable;
    clang_getCursorPlatformAvailability(cursor, nullptr, nullptr, &always_unavailable, nullptr, nullptr, 0);
    if (always_unavailable) {
        return;
    }

    switch (cursor_kind) {
        case CXCursor_TypedefDecl: {
            assert(is_on_top_level());
            assert(is_defining(cursor));

            // In Objective-C, 'id'/'Class'/'SEL' are built-in types that do not require any
            // declarations.  But typedefs with these names are allowed.  Actually, they are
            // just ignored.  On some platforms (MacOS, Linux), 'clang_getCursorType'
            // returns CXType_ObjCId/CXType_ObjCClass/CXType_ObjCSel for such cursors.  On
            // others (Windows), it returns CXType_Typedef which underlying type is
            // CXType_ObjCId/CXType_ObjCClass/CXType_ObjCSel (the actual underlying type
            // specified in the typedef is ignored).
            auto type = clang_getCursorType(cursor);
            switch (type.kind) {
                case CXType_ObjCId:
                    assert(String(clang_getCursorSpelling(cursor)).string_view() == "id");
                    break;
                case CXType_ObjCClass:
                    assert(String(clang_getCursorSpelling(cursor)).string_view() == "Class");
                    break;
                case CXType_ObjCSel:
                    assert(String(clang_getCursorSpelling(cursor)).string_view() == "SEL");
                    break;
                default: {
                    assert(type.kind == CXType_Typedef);
                    [[maybe_unused]] auto type_symbol = get_named_type<CXType_Typedef>(type, Nullability::Unspecified);
                    break;
                }
            }
            break;
        }
        case CXCursor_ObjCProtocolDecl:
            assert(!current_type());
            assert(is_on_top_level());

            // Protocol declarations are funny like that (that is, protocols are not types,
            // as well as categories).
            assert(!is_valid(clang_getCursorType(cursor)));

            assert(is_defining(cursor));
            visit(cursor, get_type_declaration(cursor, NamedTypeSymbol::Kind::Protocol));
            break;
        case CXCursor_ObjCInterfaceDecl: {
            assert(!current_type());
            assert(is_on_top_level());
            assert(is_defining(clang_getCursorType(cursor), cursor));
            visit(cursor, get_type_declaration(cursor, NamedTypeSymbol::Kind::Interface));
            break;
        }
        case CXCursor_TemplateTypeParameter: {
            assert(clang_getCursorType(cursor).kind == CXType_ObjCTypeParam);
            assert(!is_on_top_level());
            auto& decl = current_type_declaration();
            assert((parent.kind == CXCursor_ObjCInterfaceDecl && decl.is(NamedTypeSymbol::Kind::Interface)) ||
                (parent.kind == CXCursor_ObjCCategoryDecl && decl.is(NamedTypeSymbol::Kind::Category)));
            assert(is_canonical(cursor));
            assert(is_defining(cursor));

            decl.add_parameter(as_string(clang_getCursorSpelling(cursor)));
            break;
        }
        case CXCursor_ObjCCategoryDecl: {
            assert(!current_type());
            assert(!is_valid(clang_getCursorType(cursor))); // Categories are not types
            assert(is_on_top_level());
            assert(is_canonical(cursor));
            assert(is_defining(cursor));

            CXCursor interface_cursor = clang_getNullCursor();
            clang_visitChildren(
                cursor,
                [](CXCursor cursor, CXCursor, CXClientData client_data) {
                    if (clang_getCursorKind(cursor) != CXCursor_ObjCClassRef) {
                        return CXChildVisit_Recurse;
                    }
                    *static_cast<CXCursor*>(client_data) = clang_getCursorReferenced(cursor);
                    return CXChildVisit_Break; // Stop visiting once found
                },
                &interface_cursor);
            assert(!clang_Cursor_isNull(interface_cursor));
            visit(cursor,
                *new CategoryDeclarationSymbol(as_string(clang_getCursorSpelling(cursor)),
                    get_type_declaration(interface_cursor, NamedTypeSymbol::Kind::Interface)));
            break;
        }
        case CXCursor_StructDecl:
        case CXCursor_UnionDecl: {
            auto type = clang_getCursorType(cursor);
            assert(type.kind == CXType_Record);
            auto t = type_like_symbol(type);
            if (t.has_symbol_assigned()) {
                visit(cursor, t.symbol().as<TypeDeclarationSymbol>());
            } else {
                // If the return type is empty, it is an anonymous struct or union, like this:
                //
                // struct T {
                //     struct {
                //         int x;
                //     };
                // };
                //
                // Its members are considered to be members of the enclosing struct or union.
                // The structure itself is ignored and does not go to Cangjie.
                visit(cursor);
            }
            break;
        }
        case CXCursor_EnumDecl: {
            auto type = clang_getCursorType(cursor);
            assert(type.kind == CXType_Enum);
            visit(cursor, type_like_symbol(type).symbol().as<EnumDeclarationSymbol>());
            break;
        }
        case CXCursor_ObjCSuperClassRef:
            assert(!is_on_top_level());
            assert(level() == 1);
            assert(current_type_declaration().is(NamedTypeSymbol::Kind::Interface));
            assert(parent.kind == CXCursor_ObjCInterfaceDecl);
            current_type_declaration().add_base(
                type_like_symbol(clang_getCursorType(cursor)).symbol().as<TypeDeclarationSymbol>());
            break;
        case CXCursor_ObjCProtocolRef:
            // CXCursor_ObjCProtocolRef can mean a @protocol forward declaration (at the top
            // file level) or a reference to a protocol implemented by the current class,
            // category, or protocol declaration.  We only care about the latter case, that
            // is, CXCursor_ObjCProtocolRef inside CXCursor_ObjCInterfaceDecl,
            // CXCursor_ObjCCategoryDecl, or CXCursor_ObjCProtocolDecl.
            switch (parent.kind) {
                case CXCursor_ObjCInterfaceDecl:
                case CXCursor_ObjCCategoryDecl:
                case CXCursor_ObjCProtocolDecl: {
                    assert(!is_on_top_level());
                    assert(level() == 1);
                    auto& type_decl = get_target_type_declaration();
                    assert(((parent.kind == CXCursor_ObjCInterfaceDecl || parent.kind == CXCursor_ObjCCategoryDecl) &&
                               type_decl.is(TypeDeclarationSymbol::Kind::Interface)) ||
                        (parent.kind == CXCursor_ObjCProtocolDecl &&
                            type_decl.is(TypeDeclarationSymbol::Kind::Protocol)));
                    const auto referenced = clang_getCursorReferenced(cursor);
                    assert(is_valid(referenced));

                    auto& base_to_add = Universe::get()
                                            .type(TypeDeclarationSymbol::Kind::Protocol,
                                                String(clang_getCursorSpelling(referenced)).string_view())
                                            ->as<TypeDeclarationSymbol>();
                    const auto bases = type_decl.bases();
                    const bool already_has_base = std::any_of(
                        bases.begin(), bases.end(), [&base_to_add](const auto& base) { return &base == &base_to_add; });

                    if (!already_has_base) {
                        type_decl.add_base(base_to_add);
                    }
                    break;
                }
                default:
                    break;
            }
            break;
        case CXCursor_ObjCInstanceMethodDecl:
            if (is_init_method(cursor)) {
                add_constructor(cursor);
            } else {
                add_member_method(cursor, clang_Cursor_isObjCOptional(cursor) ? ModifierOptional : 0);
            }
            break;
        case CXCursor_ObjCClassMethodDecl:
            add_member_method(
                cursor, clang_Cursor_isObjCOptional(cursor) ? ModifierStatic | ModifierOptional : ModifierStatic);
            break;
        case CXCursor_ObjCPropertyDecl: {
            Modifiers modifiers = 0;
            auto attributes = clang_Cursor_getObjCPropertyAttributes(cursor, 0);
            if (attributes & CXObjCPropertyAttr_class) {
                modifiers |= ModifierStatic;
            }
            if (attributes & CXObjCPropertyAttr_readonly) {
                modifiers |= ModifierReadonly;
            }
            if (clang_Cursor_isObjCOptional(cursor)) {
                modifiers |= ModifierOptional;
            }
            add_property(as_string(clang_getCursorSpelling(cursor)),
                as_string(clang_Cursor_getObjCPropertyGetterName(cursor)),
                as_string(clang_Cursor_getObjCPropertySetterName(cursor)), modifiers);
            break;
        }
        case CXCursor_ObjCIvarDecl: {
            assert(!is_on_top_level());
            assert(is_canonical(cursor));
            assert(is_defining(cursor));
            auto access_control = cursor_to_decl<clang::ObjCIvarDecl>(cursor).getCanonicalAccessControl();
            switch (access_control) {
                case clang::ObjCIvarDecl::AccessControl::Package:
                    // Currently `package` does not go to mirrors
                    break;
                case clang::ObjCIvarDecl::AccessControl::Private:
                    break;
                case clang::ObjCIvarDecl::AccessControl::Public:
                    get_target_type_declaration().add_instance_variable(
                        as_string(clang_getCursorSpelling(cursor)), type_like_symbol(clang_getCursorType(cursor)));
                    break;
                default:
                    assert(access_control == clang::ObjCIvarDecl::AccessControl::Protected);
                    get_target_type_declaration().add_instance_variable(as_string(clang_getCursorSpelling(cursor)),
                        type_like_symbol(clang_getCursorType(cursor)), ModifierProtected);
                    break;
            }
            break;
        }
        case CXCursor_FieldDecl:
            assert(!is_on_top_level());
            assert(is_canonical(cursor));
            assert(is_defining(cursor));
            current_type_declaration().add_field(as_string(clang_getCursorSpelling(cursor)),
                type_like_symbol(clang_getCursorType(cursor)), clang_Cursor_isBitField(cursor) ? ModifierBitField : 0);
            break;
        case CXCursor_EnumConstantDecl:
            assert(!is_on_top_level());
            assert(is_canonical(cursor));
            assert(is_defining(cursor));
            current_type()->as<EnumDeclarationSymbol>().add_constant(
                as_string(clang_getCursorSpelling(cursor)), get_enum_constant_value(cursor));
            break;
        case CXCursor_FunctionDecl:
            add_top_level_function(cursor);
            break;
        case CXCursor_VarDecl:
            // We don't support variables (generic C interop) at the moment.
            // It makes sense to consider special-casing static const variables, like:
            // static const NSLayoutPriority NSLayoutPriorityDefaultHigh = 750.0;
            break;
        case CXCursor_ObjCImplementationDecl:
        case CXCursor_CompoundStmt:
            // Ignore @implementation and function bodies
            break;
        default:
            break;
    }
}

class TranslationUnit {
public:
    TranslationUnit(CXIndex index, const std::string& file, const std::vector<const char*>& args)
        : tu_(clang_parseTranslationUnit(index, file.c_str(), args.data(), static_cast<int>(args.size()), nullptr, 0,
              CXTranslationUnit_KeepGoing | CXTranslationUnit_VisitImplicitAttributes |
                  CXTranslationUnit_IncludeAttributedTypes))
    {
    }

    ~TranslationUnit()
    {
        clang_disposeTranslationUnit(tu_);
    }

    operator CXTranslationUnit() const noexcept
    {
        return tu_;
    }

private:
    const CXTranslationUnit tu_;
};

[[nodiscard]] static bool parse_source(
    CXIndex index, const std::string& file, std::vector<const char*>& args, SourceScanner& visitor)
{
    assert(!file.empty());
    TranslationUnit tu(index, file, args);
    if (!tu) {
        return false;
    }
    auto numDiagnostics = clang_getNumDiagnostics(tu);
    for (unsigned i = 0; i < numDiagnostics; ++i) {
        switch (clang_getDiagnosticSeverity(clang_getDiagnostic(tu, i))) {
            case CXDiagnostic_Error:
            case CXDiagnostic_Fatal:
                return false;
            default:
                break;
        }
    }

    visitor.visit(clang_getTranslationUnitCursor(tu));
    return true;
}

void ClangSessionImpl::parse_sources(const std::vector<std::string>& files, const std::vector<std::string>& arguments)
{
    std::vector args = {
        "-xobjective-c",
        "-fobjc-nonfragile-abi", // Required by GNUstep built for non-fragile ABI
        "-fobjc-arc",            // Prevents adding low-level staff like retain/release/NSAutoreleasePool
        "-fblocks"               // Required by GNUstep on Windows if blocks are processed
    };

    for (auto&& argument : arguments) {
        args.push_back(argument.c_str());
    }

    auto all_file_names_are_empty = true;
    for (auto&& file : files) {
        if (!file.empty()) {
            all_file_names_are_empty = false;
            if (!parse_source(index_, file, args, scanner_)) {
                fatal("Parsing failed because of compiler errors");
            }
        }
    }
    if (all_file_names_are_empty) {
        fatal("No input files");
    }
}

} // namespace objcgen
