// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "ClangSession.h"

#include <cassert>
#include <deque>
#include <filesystem>
#include <iostream>

#include <clang-c/Index.h>
#include <clang/AST/DeclBase.h>
#include <clang/AST/DeclObjC.h>
#include <clang/Basic/Version.h>

#include "FatalException.h"
#include "InputFile.h"
#include "Logging.h"
#include "Strings.h"
#include "Universe.h"

static bool operator==(const CXType& lhs, const CXType& rhs) noexcept
{
    return !!clang_equalTypes(lhs, rhs);
}

static bool operator==(const CXCursor& lhs, const CXCursor& rhs) noexcept
{
    return !!clang_equalCursors(lhs, rhs);
}

namespace objcgen {

struct CXTypeHash {
    size_t operator()(const CXType& x) const noexcept
    {
        constexpr unsigned hashBase = 31;
        // Beware: libclang implementation details
        return std::hash<void*>()(x.data[0]) * hashBase + std::hash<void*>()(x.data[1]);
    }
};

struct CXCursorHash {
    size_t operator()(const CXCursor& x) const noexcept
    {
        return clang_hashCursor(x);
    }
};

class ClangVisitor {
    static CXChildVisitResult visit(CXCursor cursor, CXCursor parent, void* data)
    {
        return static_cast<ClangVisitor*>(data)->visit_impl(cursor, parent);
    }

public:
    virtual ~ClangVisitor() = default;

    bool visit(CXCursor cursor)
    {
        return clang_visitChildren(cursor, visit, this) != 0;
    }

protected:
    virtual CXChildVisitResult visit_impl(CXCursor cursor, CXCursor parent) = 0;
};

class SourceScanner final : public ClangVisitor {
    // See the comment in CXType_ObjCTypeParam case in type_like_symbol.
    TypeDeclarationSymbol* last_objc_type_ = nullptr;

    // Nesting stack.
    std::deque<Symbol*> current_;

    // We have to name the anonymous types, use declaring file name + incrementing index suffix
    std::unordered_map<CXType, NamedTypeSymbol*, CXTypeHash> anonymous_;
    std::unordered_map<std::string, std::uint64_t> anonymous_count_;

    // libclang AST visitor visits some declarations multiple times.
    // For instance, struct X { struct { int a; } b; } will visit the inner struct twice:
    // 1. With X as parent: libclang visitor loves visiting the inner type declarations right before or after
    //                      the child that actually defines them.
    // 2. With b as parent: what one would normally expect.
    // It doesn't appear there is a way around it, other than to keep track of what we already visited.
    std::unordered_set<CXCursor, CXCursorHash> visited_;

    [[nodiscard]] NamedTypeSymbol* current_type() const
    {
        for (auto it = current_.crbegin(); it != current_.crend(); ++it) {
            if (auto* symbol = dynamic_cast<NamedTypeSymbol*>(*it)) {
                return symbol;
            }
            assert(dynamic_cast<NonTypeSymbol*>(*it));
        }
        return nullptr;
    }

    [[nodiscard]] NonTypeSymbol* current_non_type() const
    {
        for (auto it = current_.crbegin(); it != current_.crend(); ++it) {
            if (auto* symbol = dynamic_cast<NonTypeSymbol*>(*it)) {
                return symbol;
            }
            assert(dynamic_cast<NamedTypeSymbol*>(*it));
        }
        return nullptr;
    }

    [[nodiscard]] TypeDeclarationSymbol* current_type_declaration() const
    {
        auto* named_type = current_type();
        assert(named_type);
        auto* type_declaration = dynamic_cast<TypeDeclarationSymbol*>(named_type);
        assert(type_declaration);
        return type_declaration;
    }

    [[nodiscard]] std::size_t level() const
    {
        return current_.size();
    }

    [[nodiscard]] bool is_on_top_level() const
    {
        return current_.empty();
    }

    [[nodiscard]] bool current_top_is_type() const
    {
        return !current_.empty() && dynamic_cast<NamedTypeSymbol*>(current_.back());
    }

    [[nodiscard]] bool current_top_is_non_type() const
    {
        return !current_.empty() && dynamic_cast<NonTypeSymbol*>(current_.back());
    }

    [[nodiscard]] bool current_top_is_property() const noexcept
    {
        if (current_.empty()) {
            return false;
        }
        const auto* top = dynamic_cast<const NonTypeSymbol*>(current_.back());
        return top && top->kind() == NonTypeSymbol::Kind::Property;
    }

    Symbol* push_current(NamedTypeSymbol* symbol, const bool is_objc)
    {
        assert(symbol);
        assert(is_objc == (symbol->is(Kind::Interface) || symbol->is(Kind::Protocol) || symbol->is(Kind::Category)));

        current_.push_back(symbol);

        if (is_objc) {
            last_objc_type_ = current_type_declaration();
            assert(last_objc_type_);
        }

        return symbol;
    }

    Symbol* push_current(NonTypeSymbol* symbol)
    {
        assert(symbol);
        current_.push_back(symbol);
        return symbol;
    }

    Symbol* push_current(NonTypeSymbol& symbol)
    {
        return push_current(std::addressof(symbol));
    }

    void pop_current([[maybe_unused]] Symbol* symbol)
    {
        assert(symbol);
        assert(!current_.empty());
        assert(current_.back() == symbol);
        current_.pop_back();
    }

    [[nodiscard]] Symbol* push_property(
        std::string&& name, std::string&& getter, std::string&& setter, uint8_t modifiers);

    [[nodiscard]] Symbol* push_member_method(CXCursor cursor, std::string&& name, bool is_static);

    [[nodiscard]] Symbol* push_constructor(CXCursor cursor, std::string&& name);

    [[nodiscard]] NamedTypeSymbol* named_type_symbol(CXType type);

    [[nodiscard]] TypeDeclarationSymbol* type_symbol(CXType type);

    [[nodiscard]] NamedTypeSymbol* func(FuncTypeSymbol* func_type) const;

    [[nodiscard]] NamedTypeSymbol* block(CXType type);

    [[nodiscard]] std::string new_anonymous_name(CXCursor decl);

    [[nodiscard]] NamedTypeSymbol* add_type(NamedTypeSymbol::Kind kind, const std::string& name, const CXType& type);

    [[nodiscard]] TypeLikeSymbol* type_like_symbol(CXType type);

public:
    explicit SourceScanner() = default;

    SourceScanner(const SourceScanner& other) = delete;

    SourceScanner(SourceScanner&& other) noexcept = delete;

    SourceScanner& operator=(const SourceScanner& other) = delete;

    SourceScanner& operator=(SourceScanner&& other) noexcept = delete;

protected:
    CXChildVisitResult visit_impl(CXCursor cursor, CXCursor parent) override;
};

class ClangSessionImpl final : public ClangSession {
    CXIndex index_;
    SourceScanner scanner_;

public:
    ClangSessionImpl()
    {
        index_ = clang_createIndex(0, 1);
    }

    ~ClangSessionImpl();

    ClangSessionImpl(const ClangSessionImpl& other) = delete;

    ClangSessionImpl(ClangSessionImpl&& other) noexcept = delete;

    ClangSessionImpl& operator=(const ClangSessionImpl& other) = delete;

    ClangSessionImpl& operator=(ClangSessionImpl&& other) noexcept = delete;

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

[[nodiscard]] static std::string as_string(CXString string)
{
    std::string c_string = clang_getCString(string);
    clang_disposeString(string);
    return c_string;
}

[[nodiscard]] static bool is_valid(CXCursor cursor)
{
    if (clang_Cursor_isNull(cursor))
        return false;
    if (cursor.kind >= CXCursor_FirstInvalid && cursor.kind <= CXCursor_LastInvalid)
        return false;
    return true;
}

[[nodiscard]] static bool is_valid(CXType type)
{
    return type.kind != CXType_Invalid;
}

[[nodiscard]] static bool is_builtin(CXType type)
{
    assert(is_valid(type));
    const auto kind = type.kind;
    if (kind >= CXType_FirstBuiltin && kind <= CXType_LastBuiltin) {
        switch (kind) {
            case CXType_ObjCId:
            case CXType_ObjCClass:
            case CXType_ObjCSel:
                return false;
            default:
                return true;
        }
    }
    return false;
}

[[nodiscard]] static bool is_anonymous(CXCursor cursor)
{
    assert(is_valid(cursor));
    return !!clang_Cursor_isAnonymous(cursor) || !!clang_Cursor_isAnonymousRecordDecl(cursor);
}

[[nodiscard]] static bool is_canonical(CXCursor cursor)
{
    assert(is_valid(cursor));
    const auto canonical = clang_getCanonicalCursor(cursor);
    assert(is_valid(canonical));
    return cursor == canonical;
}

[[nodiscard]] static bool is_defining(CXCursor cursor)
{
    assert(is_valid(cursor));
    const auto definition = clang_getCursorDefinition(cursor);
    if (!is_valid(definition))
        return true;
    return cursor == definition;
}

[[nodiscard]] static bool is_defining(CXType type, CXCursor cursor)
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

static Location get_location(const CXSourceLocation& loc)
{
    assert(!is_null_location(loc));
    Location location;
    CXFile file;
    clang_getFileLocation(loc, &file, &location.line_, &location.col_, nullptr);
    assert(file);
    location.file_ = as_string(clang_getFileName(file));
    if (!location.file_.is_absolute()) {
        location.file_ = std::filesystem::absolute(location.file_);
    }
    return location;
}

static Location get_location(const CXCursor& decl)
{
    assert(is_valid(decl));
    auto loc = clang_getCursorLocation(decl);
    return is_null_location(loc) ? Location{{0, 0}, {}} : get_location(loc);
}

[[nodiscard]] static std::string declaring_file_name(const CXCursor& decl)
{
    assert(is_valid(decl));
    auto location = get_location(decl);
    assert(location.file_.has_stem());
    return location.file_.stem().u8string();
}

static bool set_definition_location(const CXCursor& decl, FileLevelSymbol* symbol)
{
    assert(is_valid(decl));
    assert(symbol);
    auto loc = get_location(decl);
    if (loc.is_null()) {
        return false;
    }
    symbol->set_definition_location(loc);
    return true;
}

template <class Decl>
[[nodiscard]] const std::enable_if_t<std::is_base_of_v<clang::Decl, Decl>, Decl>& cursor_to_decl(const CXCursor& cursor)
{
    const auto* decl = llvm::dyn_cast_or_null<Decl>(static_cast<const clang::Decl*>(cursor.data[0]));
    assert(decl);
    return *decl;
}

[[nodiscard]] static CXTranslationUnit cursor_to_translation_unit(const CXCursor& cursor)
{
    constexpr unsigned unitIndex = 2;
    return static_cast<CXTranslationUnit>(const_cast<void*>(cursor.data[unitIndex]));
}

[[nodiscard]] static clang::QualType type_to_qual_type(const CXType& type)
{
    return clang::QualType::getFromOpaquePtr(type.data[0]);
}

[[nodiscard]] static PrimitiveTypeCategory get_primitive_category(const CXType& type)
{
    if (type.kind == CXType_Bool) {
        // For binary compatibility with Cangjie's `Bool`, we do not support platforms
        // where `sizeof(_Bool) != 1`
        assert(clang_Type_getSizeOf(type) == 1);

        return PrimitiveTypeCategory::Boolean;
    }

    const auto cpp = type_to_qual_type(type);
    if (cpp->isUnsignedIntegerOrEnumerationType()) {
        return PrimitiveTypeCategory::UnsignedInteger;
    }
    if (cpp->isSignedIntegerOrEnumerationType()) {
        return PrimitiveTypeCategory::SignedInteger;
    }
    if (cpp->isFloatingType()) {
        return PrimitiveTypeCategory::FloatingPoint;
    }
    return PrimitiveTypeCategory::Unknown;
}

NamedTypeSymbol* SourceScanner::named_type_symbol(CXType type)
{
    auto* symbol = type_like_symbol(type);
    auto* named = dynamic_cast<NamedTypeSymbol*>(symbol);
    assert(named);
    auto* original = named->original();
    assert(original);
    assert(named->name() == original->name());
    return original;
}

TypeDeclarationSymbol* SourceScanner::type_symbol(CXType type)
{
    auto* symbol = named_type_symbol(type);
    auto* result = dynamic_cast<TypeDeclarationSymbol*>(symbol);
    assert(result);
    return result;
}

[[nodiscard]] NamedTypeSymbol* SourceScanner::func(FuncTypeSymbol* func_type) const
{
    return func_type->is_ctype() ? cfunc(func_type) : objc_func(func_type);
}

NamedTypeSymbol* SourceScanner::block(CXType type)
{
    const auto* pointee = type_like_symbol(type);
    assert(pointee->name() == "CFunc" || pointee->name() == "ObjCFunc");
    assert(dynamic_cast<const ConstructedTypeSymbol*>(pointee));
    const auto* constructedPointee = static_cast<const ConstructedTypeSymbol*>(pointee);
    assert(constructedPointee->parameter_count() == 1);
    return Universe::get()
        .type(NamedTypeSymbol::Kind::Struct, "ObjCBlock")
        ->construct({constructedPointee->parameter(0)});
}

std::string SourceScanner::new_anonymous_name(CXCursor decl)
{
    assert(is_anonymous(decl));
    auto file_name = declaring_file_name(decl);
    std::uint64_t index = 1;
    if (auto&& [item, inserted] = anonymous_count_.insert({file_name, index}); !inserted) {
        index = ++item->second;
    }
    std::string type_name = "__";
    type_name += file_name;
    type_name += "_";
    type_name += std::to_string(index);
    return type_name;
}

NamedTypeSymbol* SourceScanner::add_type(const NamedTypeSymbol::Kind kind, const std::string& name, const CXType& type)
{
    NamedTypeSymbol* symbol;
    if (kind == NamedTypeSymbol::Kind::TypeDef) {
        symbol = new TypeAliasSymbol(name);
    } else {
        symbol = new TypeDeclarationSymbol(kind, name);
    }

    if (is_builtin(type)) {
        if (auto* type_decl = dynamic_cast<TypeDeclarationSymbol*>(symbol)) {
            const auto type_category = get_primitive_category(type);
            const auto type_size = clang_Type_getSizeOf(type);
            const PrimitiveTypeInformation type_information(static_cast<size_t>(type_size), type_category);
            type_decl->set_primitive_information(type_information);
        } else {
            assert(false);
        }
    }

    if (const auto decl = clang_getTypeDeclaration(type); decl.kind != CXCursor_NoDeclFound) {
        if (is_anonymous(decl)) {
            assert(anonymous_.find(type) == anonymous_.end());
            anonymous_.emplace(type, symbol);
        }

        auto has_definition_location = set_definition_location(decl, symbol);
        if (!has_definition_location && type.kind == CXType_Typedef && name != "instancetype") {
            // The type has a declaration that has no file location.  This means a built-in
            // clang type, for example:
            //
            //   Protocol          - a built-in interface
            //   instancetype      - alias for `id`
            //   __builtin_va_list - alias for `char*`
            //   __uuint128_t      - alias for `unsigned __int128`.
            //
            // The declaration of such a type is not visited by libclang.  That is, the
            // mirror type will not be declared, which could result in cjc compiler errors.
            // If the built-in type is actually a typedef (alias), we will return its target
            // type obtained via libclang API.
            //
            // `instancetype` is a special case.  It will be replaced by the actual type
            // (not just `id`) at later stages.
            auto cx_target = clang_getTypedefDeclUnderlyingType(decl);
            if (cx_target.kind != CXType_Invalid) {
                auto* target = type_like_symbol(cx_target);
                assert(dynamic_cast<NamedTypeSymbol*>(target));
                return static_cast<NamedTypeSymbol*>(target);
            }
        }
    }

    Universe::get().register_type(symbol);

    return symbol;
}

static NamedTypeSymbol* protocol_symbol(CXType objc_object_type, unsigned i)
{
    assert(objc_object_type.kind == CXType_ObjCObject);
    assert(i < clang_Type_getNumObjCProtocolRefs(objc_object_type));
    auto protocol_decl = clang_Type_getObjCProtocolDecl(objc_object_type, i);
    assert(protocol_decl.kind == CXCursor_ObjCProtocolDecl);
    auto protocol_name = as_string(clang_getCursorSpelling(protocol_decl));
    auto& universe = Universe::get();
    auto* result = universe.type(NamedTypeSymbol::Kind::Protocol, protocol_name);
    if (!result) {
        result = new TypeDeclarationSymbol(NamedTypeSymbol::Kind::Protocol, protocol_name);
        universe.register_type(result);
        set_definition_location(protocol_decl, result);
    }
    return result;
}

struct UndecorateResult {
    std::string_view undecorated_type_name;
    std::string_view narrowing_protocol_name;
};

/**
 * The type parameter name can be specified with a narrowing protocol.  Like in
 * this sample (`T<NSCopying>` instead of just `T`):
 * <pre>
 *     @interface A<T> : NSObject
 *     - (void) foo: (T <NSCopying>) x;
 *     @end
 * </pre>
 *
 * Also, under `-fobjc-arc` the type parameter name can be prefixed with the
 * `__unsafe_unretained` or `__strong` modifier.
 *
 * <p> We need a pure name without any "decorations", to make it possible to
 * find the parameter in its owner's parameter list.  The pure name hardly can
 * be obtained with the libclang API.  Let us "undecorate" it by ourselves.
 */
static UndecorateResult undecorate_parameter_type_name(const std::string& decorated_type_name)
{
    auto without_prefix = remove_prefix(remove_prefix(decorated_type_name, "__unsafe_unretained "), "__strong ");
    auto opening_bracket = without_prefix.find('<');
    return opening_bracket == std::string_view::npos || without_prefix.back() != '>'
        ? UndecorateResult{without_prefix, std::string_view()}
        : UndecorateResult{without_prefix.substr(0, opening_bracket),
              without_prefix.substr(opening_bracket + 1, without_prefix.size() - opening_bracket - 2)};
}

TypeLikeSymbol* SourceScanner::type_like_symbol(CXType type)
{
    assert(type.kind != CXType_Invalid);

    // Libclang prior version 16 does not expose the `clang_getUnqualifiedType`
    // function.  For older clang, the type name will be cleared from
    // const\volatile\restrict manually in the end of this function;
#if CLANG_VERSION_MAJOR >= 16
    if (clang_isConstQualifiedType(type) || clang_isVolatileQualifiedType(type) ||
        clang_isRestrictQualifiedType(type)) {
        return type_like_symbol(clang_getUnqualifiedType(type));
    }
#endif

    switch (type.kind) {
            // The following are derivative classes (not definitions):

        case CXType_ObjCObject: {
            auto baseCXType = clang_Type_getObjCObjectBaseType(type);
            if (baseCXType.kind == CXType_ObjCId) {
                // This is an `id` qualified with a list of protocols
                auto* id_type = Universe::get().type(NamedTypeSymbol::Kind::Protocol, "id" /* "ObjCId" */);
                assert(id_type);
                assert(dynamic_cast<TypeDeclarationSymbol*>(id_type));
                auto num_protocols = clang_Type_getNumObjCProtocolRefs(type);
                switch (num_protocols) {
                    case 0:
                        // Should not get here, but let it go on anyway as a non-qualified `id`
                        return id_type;
                    case 1:
                        // In Cangjie, `id` qualified with just one protocol can be represented as a
                        // reference-to-interface.
                        return protocol_symbol(type, 0);
                    default:
                        auto* result = new ConstructedTypeSymbol(static_cast<TypeDeclarationSymbol*>(id_type));
                        for (decltype(num_protocols) i = 0; i < num_protocols; ++i) {
                            result->add_parameter(protocol_symbol(type, i));
                        }
                        return result;
                }
            } else {
                auto* base_type = named_type_symbol(baseCXType);
                auto type_arg_count = clang_Type_getNumObjCTypeArgs(type);
                if (type_arg_count == 0) {
                    return base_type;
                }

                auto* base_type_decl = dynamic_cast<TypeDeclarationSymbol*>(base_type);
                assert(base_type_decl);
                auto* result = new ConstructedTypeSymbol(base_type_decl);
                for (decltype(type_arg_count) i = 0; i < type_arg_count; ++i) {
                    auto* arg = type_like_symbol(clang_Type_getObjCTypeArg(type, i));
                    assert(arg);
                    result->add_parameter(arg);
                }

                return result;
            }
            break;
        }

        case CXType_ObjCObjectPointer:
            return type_like_symbol(clang_getPointeeType(type));

        case CXType_Pointer:
            return &pointer(*type_like_symbol(clang_getPointeeType(type)));

        case CXType_BlockPointer:
            return block(clang_getPointeeType(type));

        case CXType_Elaborated:
            return type_like_symbol(clang_Type_getNamedType(type));

        // Libclang bug? When CXTranslationUnit_IncludeAttributedTypes is specified, the
        // type kind of some objects is unexpectedly and incorrectly reported as
        // CXType_Unexposed rather than CXType_Attributed.  The assert below ensures
        // this is actually CXType_Attributed.
        case CXType_Unexposed:
        case CXType_Attributed: {
            auto modified_type = clang_Type_getModifiedType(type);
            assert(modified_type.kind != CXType_Invalid);
            return type_like_symbol(modified_type);
        }

        case CXType_ObjCTypeParam: {
            auto* owner_type = current_type_declaration();
            assert(owner_type);
            switch (owner_type->kind()) {
                case NamedTypeSymbol::Kind::Interface:
                case NamedTypeSymbol::Kind::Protocol:
                case NamedTypeSymbol::Kind::Category:
                    break;
                default:
                    // Non-ObjC type declarations inside ObjC interfaces/protocols in the AST
                    // are NOT children of ObjC type declaration. Therefore, current type will be the non-ObjC type,
                    // which will not have the ObjC type parameter.
                    // An example would be GSSetEnumeratorBlock struct defined in the middle of NSSet<ElementT>
                    // definition, referencing ElementT type parameter in the function pointer signature of invoke
                    // struct field. Since ObjC type declarations are top-level only, and it appears that non-ObjC
                    // declarations are located after the ObjC declarations, we can track the last ObjC declaration and
                    // use it for type parameter lookup here.
                    if (last_objc_type_) {
                        owner_type = last_objc_type_;
                        assert(owner_type->is(NamedTypeSymbol::Kind::Interface) ||
                            owner_type->is(NamedTypeSymbol::Kind::Protocol) ||
                            owner_type->is(NamedTypeSymbol::Kind::Category));
                    }
            }
            auto decorated_type_name = as_string(clang_getTypeSpelling(type));
            auto [undecorated_type_name, narrowing_protocol_name] = undecorate_parameter_type_name(decorated_type_name);
            const auto parameter_count = owner_type->parameter_count();
            for (std::size_t i = 0; i < parameter_count; ++i) {
                auto* parameter = owner_type->parameter(i);
                assert(parameter);
                auto parameter_name = parameter->name();
                if (parameter_name == undecorated_type_name) {
                    if (owner_type->kind() == NamedTypeSymbol::Kind::Category) {
                        assert(dynamic_cast<CategoryDeclarationSymbol*>(owner_type));
                        const auto* interface = static_cast<const CategoryDeclarationSymbol*>(owner_type)->interface();
                        assert(parameter_count == interface->parameter_count());
                        parameter = interface->parameter(i);
                    }
                    if (narrowing_protocol_name.empty()) {
                        return parameter;
                    }

                    // In Cangjie code, use the narrowing protocol instead of `id`
                    return new NarrowedTypeParameterSymbol(*parameter, std::string(narrowing_protocol_name));
                }
            }
            assert(false && "Unknown type parameter");
            return nullptr;
        }

        case CXType_FunctionProto: {
            auto* parameters = new TupleTypeSymbol();
            const auto num_arg_types = clang_getNumArgTypes(type);
            assert(num_arg_types >= 0);
            for (int i = 0; i < num_arg_types; ++i) {
                parameters->add_item(type_like_symbol(clang_getArgType(type, i)));
            }
            auto* return_type = type_like_symbol(clang_getResultType(type));
            return func(new FuncTypeSymbol(parameters, return_type));
        }

        case CXType_IncompleteArray:
            return new VArraySymbol(*type_like_symbol(clang_getArrayElementType(type)), 0);

        case CXType_ConstantArray:
            return new VArraySymbol(
                *type_like_symbol(clang_getArrayElementType(type)), static_cast<size_t>(clang_getArraySize(type)));

            // We will handle the rest below this switch.

        default:;
    }

    if (const auto it = anonymous_.find(type); it != anonymous_.end()) {
        return it->second;
    }

    // This is a type which requires definition.

    std::string type_name;
    auto type_kind = NamedTypeSymbol::Kind::Undefined;

    switch (type.kind) {
        case CXType_ObjCId:
            return Universe::get().type(NamedTypeSymbol::Kind::Protocol, "id" /* "ObjCId" */);
        case CXType_ObjCClass:
            return Universe::get().type(NamedTypeSymbol::Kind::Interface, "Class" /* "ObjCClass" */);
        case CXType_ObjCSel:
            return Universe::get().type(NamedTypeSymbol::Kind::Interface, "SEL" /* "ObjCSelector" */);
        case CXType_Typedef:
            type_kind = NamedTypeSymbol::Kind::TypeDef;
            type_name = as_string(clang_getTypeSpelling(type));
            // TODO: clang_getCanonicalType(type) if needed
            break;

        case CXType_ObjCInterface: {
            type_name = as_string(clang_getTypeSpelling(type));
            type_kind = NamedTypeSymbol::Kind::Interface;
            assert(clang_getCanonicalType(type) == type);
            break;
        }

        case CXType_Record: {
            const auto decl = clang_getTypeDeclaration(type);
            assert(is_valid(decl));
            switch (decl.kind) {
                case CXCursor_StructDecl:
                    type_kind = NamedTypeSymbol::Kind::Struct;
                    break;
                case CXCursor_UnionDecl:
                    type_kind = NamedTypeSymbol::Kind::Union;
                    break;
                default:
                    assert(false);
            }
            if (is_anonymous(decl)) {
                type_name = new_anonymous_name(decl);
            } else {
                type_name = as_string(clang_getTypeSpelling(type));
                switch (type_kind) {
                    case NamedTypeSymbol::Kind::Struct:
                        remove_prefix_in_place(type_name, "struct ");
                        break;
                    case NamedTypeSymbol::Kind::Union:
                        remove_prefix_in_place(type_name, "union ");
                        break;
                    default:
                        assert(false);
                }
            }
            break;
        }

        case CXType_Enum: {
            type_kind = NamedTypeSymbol::Kind::Enum;
            const auto decl = clang_getTypeDeclaration(type);
            if (is_anonymous(decl)) {
                type_name = new_anonymous_name(decl);
            } else {
                type_name = as_string(clang_getTypeSpelling(type));
                remove_prefix_in_place(type_name, "enum ");
            }
            break;
        }

            // Anything else is not supported at the moment.

        default:
            if (is_builtin(type)) {
                type_kind = NamedTypeSymbol::Kind::SourcePrimitive;
                type_name = as_string(clang_getTypeSpelling(type));
                break;
            }

            assert(false);
            return nullptr;
    }

#if CLANG_VERSION_MAJOR < 16
    remove_prefix_in_place(type_name, "const ");
    remove_prefix_in_place(type_name, "volatile ");
    if (ends_with(type_name, "*restrict")) {
        remove_prefix_in_place(type_name, "restrict");
    }
#endif

    if (auto* type_symbol = Universe::get().type(type_kind, type_name)) {
        return type_symbol;
    }

    return this->add_type(type_kind, type_name, type);
}

Symbol* SourceScanner::push_property(std::string&& name, std::string&& getter, std::string&& setter, uint8_t modifiers)
{
    assert(current_top_is_type());
    auto* decl = current_type_declaration();
    if (decl->is(NamedTypeSymbol::Kind::Category)) {
        assert(dynamic_cast<CategoryDeclarationSymbol*>(decl));
        decl = static_cast<CategoryDeclarationSymbol*>(decl)->interface();
    }
    return push_current(decl->add_property(std::move(name), getter, setter, modifiers));
}

static bool is_init_method(CXCursor cursor) noexcept
{
    assert(cursor.kind == CXCursor_ObjCInstanceMethodDecl);
    return cursor_to_decl<clang::ObjCMethodDecl>(cursor).getMethodFamily() == clang::ObjCMethodFamily::OMF_init;
}

class OverriddenCursors final {
public:
    [[nodiscard]] static OverriddenCursors get(CXCursor cursor)
    {
        CXCursor* overridden_cursors;
        unsigned num_overridden_cursors;
        clang_getOverriddenCursors(cursor, &overridden_cursors, &num_overridden_cursors);
        assert((num_overridden_cursors != 0) == (overridden_cursors != nullptr));
        return OverriddenCursors(overridden_cursors, num_overridden_cursors);
    }

    OverriddenCursors(const OverriddenCursors&) = delete;

    ~OverriddenCursors()
    {
        clang_disposeOverriddenCursors(overridden_cursors_);
    }

    [[nodiscard]] const CXCursor* begin() const noexcept
    {
        return overridden_cursors_;
    }

    [[nodiscard]] const CXCursor* end() const noexcept
    {
        return end_;
    }

private:
    CXCursor* const overridden_cursors_;
    const CXCursor* const end_;

    OverriddenCursors(CXCursor* overridden_cursors, unsigned num_overridden_cursors) noexcept
        : overridden_cursors_(overridden_cursors), end_(overridden_cursors + num_overridden_cursors)
    {
    }
};

// Return true if the corresponding type in the Cangjie code must be prefixed
// with '?' (wrapped by `std.Option`).
static bool is_nullable(CXType type)
{
    switch (type.kind) {
        // Libclang bug? When CXTranslationUnit_IncludeAttributedTypes is specified, the
        // type kind of some objects is unexpectedly and incorrectly reported as
        // CXType_Unexposed rather than CXType_Attributed.  The assert below ensures
        // this is actually CXType_Attributed.
        case CXType_Unexposed:
        case CXType_Attributed: {
            auto modified_type_kind = clang_Type_getModifiedType(type).kind;
            assert(modified_type_kind != CXType_Invalid);

            if (modified_type_kind == CXType_ObjCObjectPointer) {
                return clang_Type_getNullability(type) != CXTypeNullability_NonNull;
            }

            // This will be most probably converted to CPointer.  In Objective-C, C pointer
            // can be annotated as nullable/nonnull.  But in Cangjie, CPointer is always
            // nullable, there is no sense to make it optional.
            return false;
        }
        case CXType_ObjCObjectPointer:
        case CXType_ObjCId:
        case CXType_ObjCClass:
        case CXType_ObjCSel:
            return true;
        default:
            return false;
    }
}

static std::optional<CXType> nullable_overridden(CXCursor cursor)
{
    for (const auto& overridden_cursor : OverriddenCursors::get(cursor)) {
        auto overridden_result_type = nullable_overridden(overridden_cursor);
        if (overridden_result_type) {
            return overridden_result_type;
        }
        auto ort = clang_getCursorResultType(overridden_cursor);
        if (is_nullable(ort)) {
            return ort;
        }
    }
    return std::nullopt;
}

Symbol* SourceScanner::push_member_method(CXCursor cursor, std::string&& name, bool is_static)
{
    assert(current_top_is_type() || current_top_is_property());
    auto* decl = current_type_declaration();
    if (decl->is(NamedTypeSymbol::Kind::Category)) {
        assert(dynamic_cast<CategoryDeclarationSymbol*>(decl));
        decl = static_cast<CategoryDeclarationSymbol*>(decl)->interface();
    }
    auto cx_result_type = clang_getCursorResultType(cursor);

    // In Cangjie, Option is not covariant.  If either overridden or overrider is
    // nullable, do not change the result type of the overrider.
    auto nullable_overridden_result_type = nullable_overridden(cursor);
    if (nullable_overridden_result_type) {
        cx_result_type = *nullable_overridden_result_type;
    }
    if (cx_result_type.kind == CXType_ObjCId) {
        for (const auto& overridden_cursor : OverriddenCursors::get(cursor)) {
            auto overriden_result_type = clang_getCursorResultType(overridden_cursor);
            if (overriden_result_type.kind != CXType_ObjCId) {
                // In Objective-C, contravariant return types are allowed. That will not compile
                // in Cangjie, as `id` is not a subtype of pointer-to-class. To make it
                // compilable, do not change the result type in such cases.
                cx_result_type = overriden_result_type;
                break;
            }
        }
    }
    uint8_t modifiers = 0;
    if (is_static) {
        modifiers |= ModifierStatic;
    }
    if (is_nullable(cx_result_type)) {
        modifiers |= ModifierNullable;
    }
    return push_current(decl->add_member_method(std::move(name), type_like_symbol(cx_result_type), modifiers));
}

Symbol* SourceScanner::push_constructor(CXCursor cursor, std::string&& name)
{
    assert(current_top_is_type());
    auto result_type = clang_getCursorResultType(cursor);
    auto* decl = current_type_declaration();
    if (decl->is(NamedTypeSymbol::Kind::Category)) {
        assert(dynamic_cast<CategoryDeclarationSymbol*>(decl));
        decl = static_cast<CategoryDeclarationSymbol*>(decl)->interface();
    }
    return push_current(decl->add_constructor(std::move(name), type_like_symbol(result_type)));
}

CXChildVisitResult SourceScanner::visit_impl(CXCursor cursor, CXCursor parent)
{
    assert(is_valid(cursor));
    assert(is_valid(parent));

    const auto cursor_kind = clang_getCursorKind(cursor);
    auto name = as_string(clang_getCursorSpelling(cursor));
    const auto type = clang_getCursorType(cursor);

    const auto [_, first_visit] = visited_.emplace(cursor);

    if (verbosity >= LogLevel::DEBUG) {
        const auto level = this->level();
        for (std::size_t i = 0; i < level; i++) {
            std::cout << ' ';
        }

        std::cout << as_string(clang_getCursorKindSpelling(cursor_kind)) << ' ' << name;

        if (type.kind != CXType_Invalid) {
            std::cout << " <" << as_string(clang_getTypeSpelling(type)) << ">";
        }

        if (is_anonymous(cursor)) {
            std::cout << " [anonymous]";
        }

        if (!first_visit) {
            std::cout << " [visited]";
        }

        std::cout << std::endl;
    }

    if (!first_visit) {
        return CXChildVisit_Continue;
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
    // TODO: take into account particular platform?
    int always_unavailable;
    clang_getCursorPlatformAvailability(cursor, nullptr, nullptr, &always_unavailable, nullptr, nullptr, 0);
    if (always_unavailable) {
        return CXChildVisit_Continue;
    }

    if (!inputs.add_cursor(get_location(cursor), name)) {
        // This cursor already has been processed in one of the previous translations.
        // Omit it to avoid redefinitions.
        return CXChildVisit_Continue;
    }

    Symbol* pushed = nullptr;
    auto recurse = true;

    switch (cursor_kind) {
        case CXCursor_TypedefDecl: {
            assert(is_on_top_level());
            assert(is_defining(cursor));
            auto* def = named_type_symbol(type);
            switch (def->kind()) {
                case NamedTypeSymbol::Kind::TypeDef:
                    assert(dynamic_cast<TypeAliasSymbol*>(def));
                    static_cast<TypeAliasSymbol*>(def)->set_target(
                        type_like_symbol(clang_getTypedefDeclUnderlyingType(cursor)));
                    break;
                case NamedTypeSymbol::Kind::Protocol:
                    // This is the type `id`.  It is specially processed by the generator, ignore
                    // the declaration.
                    assert(name == "id");
                    return CXChildVisit_Continue;
                case NamedTypeSymbol::Kind::Interface:
                    // This is one of the types that are specially processed by the generator,
                    // ignore the declaration.
                    assert(name == "SEL" || name == "Class");
                    return CXChildVisit_Continue;
                case NamedTypeSymbol::Kind::Struct:
                case NamedTypeSymbol::Kind::Union:
                case NamedTypeSymbol::Kind::Enum:
                    // typedef struct MyStruct { ... } MyStruct;
                    break;
                default:
                    assert(false);
                    break;
            }
            recurse = false;
            break;
        }
        case CXCursor_ObjCProtocolDecl: {
            assert(!current_type());
            assert(is_on_top_level());
            assert(!is_valid(type)); // Protocol declarations are funny like that.
            assert(is_defining(cursor));
            auto& universe = Universe::get();
            auto* decl = universe.type(NamedTypeSymbol::Kind::Protocol, name);
            if (!decl) {
                decl = new TypeDeclarationSymbol(NamedTypeSymbol::Kind::Protocol, name);
                universe.register_type(decl);
                set_definition_location(cursor, decl);
            }
            pushed = push_current(decl, true);
            break;
        }
        case CXCursor_ObjCInterfaceDecl: {
            assert(!current_type());
            assert(is_on_top_level());
            assert(is_defining(type, cursor));
            auto& universe = Universe::get();
            auto* named = universe.type(NamedTypeSymbol::Kind::Interface, name);
            if (!named) {
                auto* decl = new TypeDeclarationSymbol(NamedTypeSymbol::Kind::Interface, name);
                universe.register_type(decl);
                set_definition_location(cursor, decl);
                named = decl;
            }
            pushed = push_current(named, true);
            break;
        }
        case CXCursor_TemplateTypeParameter: {
            if (is_valid(type) && type.kind == CXType_ObjCTypeParam && is_valid(parent)) {
                switch (parent.kind) {
                    case CXCursor_ObjCInterfaceDecl:
                    case CXCursor_ObjCCategoryDecl:
                        assert(current_top_is_type());

                        {
                            auto* decl = current_type_declaration();
                            assert((parent.kind == CXCursor_ObjCInterfaceDecl &&
                                       decl->is(NamedTypeSymbol::Kind::Interface)) ||
                                (parent.kind == CXCursor_ObjCCategoryDecl &&
                                    decl->is(NamedTypeSymbol::Kind::Category)));
                            assert(is_canonical(cursor));
                            assert(is_defining(cursor));

                            decl->add_parameter(name);
                        }
                        break;
                    default:
                        break;
                }
            }
            break;
        }
        case CXCursor_ObjCCategoryDecl: {
            assert(!current_type());
            assert(!is_valid(type));
            assert(is_on_top_level());
            assert(is_canonical(cursor));
            assert(is_defining(cursor));
            const auto& cpp = cursor_to_decl<clang::ObjCCategoryDecl>(cursor);
            auto* interface_decl = cpp.getClassInterface();
            assert(interface_decl->getKind() == clang::Decl::ObjCInterface);

            CXCursor target = {};
            target.kind = CXCursor_ObjCInterfaceDecl;
            target.data[0] = interface_decl;
            target.data[2] = cursor.data[2];

            const auto interface_type = clang_getCursorType(target);
            assert(interface_type.kind == CXType_ObjCInterface);
            auto* decl = named_type_symbol(interface_type);
            assert(dynamic_cast<TypeDeclarationSymbol*>(decl));
            pushed = push_current(
                new CategoryDeclarationSymbol(std::string(cpp.getName()), static_cast<TypeDeclarationSymbol*>(decl)),
                true);
            break;
        }
        case CXCursor_StructDecl:
        case CXCursor_UnionDecl: {
            assert(type.kind == CXType_Record);
            auto* decl = named_type_symbol(type);
            pushed = push_current(decl, false);
            break;
        }
        case CXCursor_EnumDecl: {
            assert(type.kind == CXType_Enum);
            auto* decl = named_type_symbol(type);
            pushed = push_current(decl, false);
            break;
        }
        case CXCursor_ObjCSuperClassRef: {
            assert(current_top_is_type());
            assert(level() == 1);
            assert(current_type_declaration()->is(NamedTypeSymbol::Kind::Interface));
            assert(parent.kind == CXCursor_ObjCInterfaceDecl);
            current_type_declaration()->add_base(type_like_symbol(type));
            break;
        }
        case CXCursor_ObjCProtocolRef: {
            if (parent.kind == CXCursor_TranslationUnit && is_on_top_level()) {
                // For some reason libclang cursors replace forward declarations of
                // CXCursor_ObjCInterfaceDecl and CXCursor_ObjCProtocolDecl
                // with their ClassRef and ProtocolRef respectively.
                break;
            }

            // We only care about CXCursor_ObjCProtocolRef _inside_ CXCursor_ObjCInterfaceDecl definition,
            // which means it is a base type of the definition.
            const auto is_interface = parent.kind == CXCursor_ObjCInterfaceDecl;
            const auto is_protocol = parent.kind == CXCursor_ObjCProtocolDecl;
            if (is_interface || is_protocol) {
                using Kind = NamedTypeSymbol::Kind;
                assert(current_top_is_type());
                assert(level() == 1);
                auto* type_decl = current_type_declaration();
                [[maybe_unused]] const auto kind = type_decl->kind();
                assert((is_interface && kind == Kind::Interface) || (is_protocol && kind == Kind::Protocol));
                const auto referenced = clang_getCursorReferenced(cursor);
                assert(is_valid(referenced));
                const auto referenced_type = as_string(clang_getCursorSpelling(referenced));
                auto* protocol = Universe::get().type(Kind::Protocol, referenced_type);
                type_decl->add_base(protocol);
            }

            break;
        }
        case CXCursor_ObjCInstanceMethodDecl: {
            pushed = is_init_method(cursor) ? push_constructor(cursor, std::move(name))
                                            : push_member_method(cursor, std::move(name), false);
            break;
        }
        case CXCursor_ObjCClassMethodDecl: {
            pushed = push_member_method(cursor, std::move(name), true);
            break;
        }
        case CXCursor_ObjCPropertyDecl: {
            auto attributes = clang_Cursor_getObjCPropertyAttributes(cursor, 0);
            pushed = push_property(std::move(name), as_string(clang_Cursor_getObjCPropertyGetterName(cursor)),
                as_string(clang_Cursor_getObjCPropertySetterName(cursor)),
                static_cast<uint8_t>((attributes & CXObjCPropertyAttr_class ? ModifierStatic : 0) |
                    (attributes & CXObjCPropertyAttr_readonly ? ModifierReadonly : 0)));
            break;
        }
        case CXCursor_ObjCIvarDecl: {
            assert(current_top_is_type());
            assert(is_canonical(cursor));
            assert(is_defining(cursor));
            auto access_control = cursor_to_decl<clang::ObjCIvarDecl>(cursor).getCanonicalAccessControl();
            switch (access_control) {
                case clang::ObjCIvarDecl::AccessControl::Package:
                    // TODO?
                    recurse = false;
                    break;
                case clang::ObjCIvarDecl::AccessControl::Private:
                    recurse = false;
                    break;
                case clang::ObjCIvarDecl::AccessControl::Public:
                    pushed = push_current(current_type_declaration()->add_instance_variable(
                        name, type_like_symbol(type), is_nullable(type) ? ModifierNullable : 0));
                    break;
                default:
                    assert(access_control == clang::ObjCIvarDecl::AccessControl::Protected);
                    pushed = push_current(current_type_declaration()->add_instance_variable(
                        name, type_like_symbol(type), ModifierProtected | (is_nullable(type) ? ModifierNullable : 0)));
                    break;
            }
            break;
        }
        case CXCursor_FieldDecl: {
            assert(current_top_is_type());
            assert(is_canonical(cursor));
            assert(is_defining(cursor));
            auto& field = current_type_declaration()->add_field(name, type_like_symbol(type), is_nullable(type));
            if (clang_Cursor_isBitField(cursor)) {
                auto width = clang_getFieldDeclBitWidth(cursor);
                assert(width >= 0);
                assert(width < 0xFF);
                field.set_bit_field_size(static_cast<uint8_t>(width));
            }
            pushed = push_current(field);
            break;
        }
        case CXCursor_EnumConstantDecl: {
            assert(current_top_is_type());
            assert(is_canonical(cursor));
            assert(is_defining(cursor));
            auto& constant = current_type_declaration()->add_enum_constant(name, type_like_symbol(type));
            pushed = push_current(constant);
            constant.set_enum_constant_value(clang_getEnumConstantDeclUnsignedValue(cursor));
            break;
        }
        case CXCursor_ParmDecl: {
            assert(is_canonical(cursor));
            assert(is_defining(cursor));
            if (current_top_is_non_type()) {
                auto& non_type = *current_non_type();
                if (non_type.is_member_method() || non_type.is_constructor()) {
                    // e.g. comparator function pointers in parameters
                    non_type.add_parameter(name, type_like_symbol(type), is_nullable(type));
                    recurse = false;
                } else if (non_type.is_property()) {
                    recurse = false;
                }
            }
            break;
        }
        case CXCursor_VarDecl:
        case CXCursor_FunctionDecl: {
            // We don't support functions/variables (generic C interop) at the moment.
            // TODO: consider special-casing static const variables, like:
            // static const NSLayoutPriority NSLayoutPriorityDefaultHigh = 750.0;
            recurse = false;
            break;
        }
        case CXCursor_ObjCImplementationDecl:
            // Ignore @implementation
            return CXChildVisit_Continue;
        default:;
    }

    if (recurse) {
        visit(cursor);
    }

    if (pushed) {
        pop_current(pushed);
    }

    return CXChildVisit_Continue;
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

static bool parse_source(CXIndex index, const std::string& file, std::vector<const char*>& args, SourceScanner& visitor)
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

    const auto cursor = clang_getTranslationUnitCursor(tu);

    inputs.next_translation();
    visitor.visit(cursor);
    return true;
}

void ClangSessionImpl::parse_sources(const std::vector<std::string>& files, const std::vector<std::string>& arguments)
{
    std::vector args = {"-xobjective-c", "-fobjc-arc"};

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
