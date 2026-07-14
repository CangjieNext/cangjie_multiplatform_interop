// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "Transform.h"

#include <iostream>

#include "Logging.h"
#include "Mappings.h"
#include "Universe.h"

namespace objcgen {

[[nodiscard]] static bool is_instancetype(const Type& type)
{
    const auto& type_symbol = type.symbol();
    bool yes = type_symbol.name() == "instancetype";
    assert(!yes || &type_symbol.as<TypeAliasSymbol>().target().symbol() == &Universe::get().id());
    return yes;
}

static void replace_return_instancetype(TypeDeclarationSymbol& decl, NonTypeSymbol& method, Nullability nullability)
{
    std::vector<Type> args;
    args.reserve(decl.parameter_count());
    for (auto& parameter : decl.parameters()) {
        args.emplace_back(parameter, nullability);
    }
    method.set_return_type({decl, std::move(args), nullability});
}

// - Replace `instancetype` by the declaring class type.
// - Replace the constructor return type by the strictly nonnull declaring class
//   type (that is a cjc frontend requirement).
static void replace_instancetype(TypeDeclarationSymbol& decl)
{
    assert(decl.is(NamedTypeSymbol::Kind::Interface) || decl.is(NamedTypeSymbol::Kind::Protocol));
    for (auto& member : decl.members()) {
        switch (member.kind()) {
            case NonTypeSymbol::Kind::Constructor:
                // For 'init' methods, the cjc frontend requires the return type to be strictly
                // the declaring class, and the nullability must be nonnull.
                replace_return_instancetype(decl, member, Nullability::Nonnull);
                break;
            case NonTypeSymbol::Kind::MemberMethod: {
                // For non-@ObjCInit methods, `instancetype` is mapped to the declaring class,
                // keeping the original nullability.
                const auto& original_return_type = member.return_type();
                if (is_instancetype(original_return_type)) {
                    replace_return_instancetype(decl, member, original_return_type.nullability());
                }
            }
            default:
                break;
        }
    }
}

// Rename the method and update properties attached to it, if any
static void rename_method(TypeDeclarationSymbol& decl, NonTypeSymbol& method, std::string new_name)
{
    assert(method.is_member_method());
    auto is_static = method.is_static();
    const auto& selector = method.selector();
    for (auto& member : decl.members()) {
        if (!member.is_property() || member.is_static() != is_static) {
            continue;
        }
        if (member.getter() == selector) {
            if (member.selector() == selector) {
                member.rename(new_name);
                member.rename_getter({});
            } else {
                member.rename_getter(new_name);
            }
        } else if (member.setter() == selector) {
            member.rename_setter(new_name);
        } else if (member.selector() == selector) {
            member.rename(new_name);
        }
    }
    method.rename(std::move(new_name));
}

static void resolve_static_instance_clash(TypeDeclarationSymbol& decl, NonTypeSymbol& method, bool is_static)
{
    assert(method.is_member_method());
    rename_method(decl, method, method.name() + (is_static ? "Static" : "Instance"));
}

struct StaticInstancePair {
public:
    void add(NonTypeSymbol& type) noexcept;

    [[nodiscard]] bool both() const noexcept
    {
        return static_ && instance_;
    }

    [[nodiscard]] NonTypeSymbol* get_static() const noexcept
    {
        return static_;
    }

    [[nodiscard]] const NonTypeSymbol* get_instance() const noexcept
    {
        return instance_;
    }

private:
    NonTypeSymbol* static_;
    const NonTypeSymbol* instance_;
};

void StaticInstancePair::add(NonTypeSymbol& type) noexcept
{
    if (type.is_static()) {
        assert(!static_ && "Cannot be multiple static methods with the same name");
        static_ = &type;
    } else {
        assert(!instance_ && "Cannot be multiple instance methods with the same name");
        instance_ = &type;
    }
}

static NonTypeSymbol& get_getter(TypeDeclarationSymbol& decl, const NonTypeSymbol& property)
{
    assert(property.is_property());
    auto is_static = property.is_static();
    const auto& getter = property.getter();
    auto members = decl.members();
    auto e = members.end();
    auto it = std::find_if(members.begin(), e, [is_static, &getter](auto& member) {
        return member.is_member_method() && member.is_static() == is_static && member.selector() == getter;
    });
    assert(it != e);
    return *it;
}

static NonTypeSymbol& get_setter(TypeDeclarationSymbol& decl, const NonTypeSymbol& property)
{
    assert(property.is_property());
    assert(!property.is_readonly());
    auto is_static = property.is_static();
    const auto& setter = property.setter();
    auto members = decl.members();
    auto e = members.end();
    auto it = std::find_if(members.begin(), e, [is_static, &setter](auto& member) {
        return member.is_member_method() && member.is_static() == is_static && member.selector() == setter;
    });
    assert(it != e);
    return *it;
}

static void transform_type(TypeDeclarationSymbol& decl)
{
    auto type_kind = decl.kind();
    switch (type_kind) {
        case NamedTypeSymbol::Kind::Protocol: {
            replace_instancetype(decl);

            // If the protocol clashes by name with a non-protocol global symbol, rename the
            // protocol by adding as many "Protocol" suffixes as needed for its uniqueness
            // among all global symbols.  Note that the results may depend on the order of
            // declarations and on the current closure specified in the configuration.
            auto& universe = Universe::get();
            std::string_view name = decl.name();
            if (!universe.type(TypeNamespace::Primary, name) && !universe.type(TypeNamespace::Tagged, name) &&
                !universe.global_non_type_symbol(name)) {
                break;
            }
            auto new_name = std::string(name);
            do {
                new_name += "Protocol";
            } while (universe.type(TypeNamespace::Primary, new_name) ||
                universe.type(TypeNamespace::Tagged, new_name) || universe.type(TypeNamespace::Protocols, new_name));
            if (verbosity >= LogLevel::INFO) {
                std::cerr << "Renaming clashing protocol `" << name << "` to `" << new_name << '`' << std::endl;
            }
            universe.rename_type(decl, std::move(new_name));
            break;
        }
        case NamedTypeSymbol::Kind::Interface:
            replace_instancetype(decl);
            break;
        case NamedTypeSymbol::Kind::Struct:
        case NamedTypeSymbol::Kind::Union:
        case NamedTypeSymbol::Kind::Enum: {
            // If the tagged type clashes by name with a non-tagged global symbol, rename
            // the type by adding as many "Struct"/"Union"/"Enum" suffixes as needed for
            // its uniqueness among all global symbols.  Note that the results may depend on
            // the order of declarations and on the current closure specified in the
            // configuration.
            auto& universe = Universe::get();
            std::string_view name = decl.name();
            if (!universe.type(TypeNamespace::Primary, name) && !universe.type(TypeNamespace::Protocols, name) &&
                !universe.global_non_type_symbol(name)) {
                break;
            }
            auto new_name = std::string(name);
            const char* suffix;
            switch (type_kind) {
                case NamedTypeSymbol::Kind::Enum:
                    suffix = "Enum";
                    break;
                case NamedTypeSymbol::Kind::Union:
                    suffix = "Union";
                    break;
                default:
                    assert(type_kind == NamedTypeSymbol::Kind::Struct);
                    suffix = "Struct";
                    break;
            };
            do {
                new_name += suffix;
            } while (universe.type(TypeNamespace::Primary, new_name) ||
                universe.type(TypeNamespace::Protocols, new_name) || universe.type(TypeNamespace::Tagged, new_name));
            if (verbosity >= LogLevel::INFO) {
                const char* tag;
                switch (type_kind) {
                    case NamedTypeSymbol::Kind::Enum:
                        tag = "enum";
                        break;
                    case NamedTypeSymbol::Kind::Union:
                        tag = "union";
                        break;
                    default:
                        assert(type_kind == NamedTypeSymbol::Kind::Struct);
                        tag = "struct";
                        break;
                };
                std::cerr << "Renaming clashing `" << tag << ' ' << name << "` to `" << new_name << '`' << std::endl;
            }
            universe.rename_type(decl, std::move(new_name));
            break;
        }
        default:
            break;
    }

    auto members = decl.members();

    // Resolve static/instance clashes inside 'decl'
    std::unordered_map<std::string_view, StaticInstancePair> map;
    for (auto& member : members) {
        if (member.is_member_method()) {
            map[member.selector()].add(member);
        }
    }
    for (const auto& [name, methods] : map) {
        if (methods.both()) {
            auto& static_method = *methods.get_static();
            assert(static_method.selector() == methods.get_instance()->selector());
            resolve_static_instance_clash(decl, static_method, true);
        }
    }

    // Hide getters/setters
    for (const auto& member : members) {
        if (member.is_property()) {
            get_getter(decl, member).set_hidden();
            if (!member.is_readonly()) {
                get_setter(decl, member).set_hidden();
            }
        }
    }
}

[[nodiscard]] static bool is_base_of(const TypeDeclarationSymbol& base, const TypeDeclarationSymbol& derived) noexcept
{
    if (&base == &derived) {
        return true;
    }
    for (const auto& b : derived.bases()) {
        if (is_base_of(base, b)) {
            return true;
        }
    }
    return false;
}

static void transform_base_derived(const TypeDeclarationSymbol& base, TypeDeclarationSymbol& derived)
{
    for (auto& derived_member : derived.members()) {
        switch (derived_member.kind()) {
            case NonTypeSymbol::Kind::MemberMethod:
                for (const auto& base_member : base.members()) {
                    if (!base_member.is_member_method() || base_member.selector() != derived_member.selector()) {
                        continue;
                    }

                    // Resolve the following clashes in override method return types:
                    //
                    // - In Cangjie, Option is not covariant.  If 'base_member' and 'derived_member'
                    //   have different nullabilities, change the nullability of the derived return
                    //   type.
                    // - In Cangjie, Option is not covariant.  If both 'base_member' and
                    //   'derived_member' are nullable, ensure that 'derived_member' has the same
                    //   return type as 'base_member'.
                    // - In Objective-C, contravariant return types are allowed.  That will not
                    //   compile in Cangjie. Change the return type of 'derived_member' accordingly.
                    if (base_member.is_static() == derived_member.is_static()) {
                        derived_member.set_override();
                        const auto& base_member_type = base_member.return_type();
                        auto& derived_member_type = derived_member.return_type();
                        if (base_member_type.nullability() != Nullability::Nonnull) {
                            if (derived_member_type.nullability() == Nullability::Nonnull) {
                                derived_member_type.set_nullability(Nullability::Nullable);
                            }

                            // Both are Option.  Must be the same type.
                            derived_member.set_return_type(base_member_type);
                        } else {
                            if (derived_member_type.nullability() != Nullability::Nonnull) {
                                derived_member_type.set_nullability(Nullability::Nonnull);
                            }

                            // Both are non-Option.  Either they must be the same or the overridden must be
                            // a base of the overrider.
                            if (is_instancetype(derived_member_type)) {
                                // For non-init methods, 'instancetype' is mapped to the declaring class
                                replace_return_instancetype(derived, derived_member, Nullability::Nonnull);
                            } else {
                                const auto* derived_member_type_decl = dynamic_cast<const TypeDeclarationSymbol*>(
                                    &derived_member_type.canonical_type_symbol());
                                if (derived_member_type_decl) {
                                    const auto* base_member_type_decl = dynamic_cast<const TypeDeclarationSymbol*>(
                                        &base_member_type.canonical_type_symbol());
                                    if (base_member_type_decl &&
                                        !is_base_of(*base_member_type_decl, *derived_member_type_decl)) {
                                        derived_member.set_return_type(base_member_type);
                                    }
                                }
                            }
                        }
                    }

                    const auto& base_name = base_member.name();
                    if (base_name == derived_member.name()) {
                        // Resolve static/instance name clashes
                        if (derived_member.is_static()) {
                            if (!base_member.is_static()) {
                                resolve_static_instance_clash(derived, derived_member, true);
                            }
                        } else if (base_member.is_static()) {
                            resolve_static_instance_clash(derived, derived_member, false);
                        }
                    } else {
                        // The base member has been renamed.  Rename the derived accordingly.
                        derived_member.rename(base_name);
                    }
                }
                break;
            case NonTypeSymbol::Kind::Constructor:
                for (const auto& base_member : base.members()) {
                    if (base_member.is_constructor() && base_member.selector() == derived_member.selector()) {
                        derived_member.set_override();
                    }
                }
                break;
            default:
                break;
        }
    }

    // Make hidden (non-printed):
    // - Overridden properties.
    // - Properties conflicting by name with a base method.
    // - Methods conflicting by name with a base property.
    for (auto& derived_member : derived.members()) {
        switch (derived_member.kind()) {
            case NonTypeSymbol::Kind::Property: {
                const auto& derived_name = derived_member.name();
                for (const auto& base_member : base.members()) {
                    switch (base_member.kind()) {
                        case NonTypeSymbol::Kind::Property:
                            if (derived_name == base_member.name()) {
                                derived_member.set_hidden();
                            }
                            break;
                        case NonTypeSymbol::Kind::MemberMethod:
                            if (derived_name == base_member.name()) {
                                derived_member.set_hidden();
                            }
                            break;
                        default:
                            break;
                    }
                }
                break;
            }
            case NonTypeSymbol::Kind::MemberMethod: {
                const auto& derived_name = derived_member.name();
                for (const auto& base_member : base.members()) {
                    if (base_member.is_property() && derived_name == base_member.name()) {
                        derived_member.set_hidden();
                    }
                }
                break;
            }
            default:
                break;
        }
    }
}

static void transform_visit(TypeDeclarationSymbol& decl);

static void transform_visit(TypeDeclarationSymbol& base, TypeDeclarationSymbol& derived)
{
    transform_visit(base);

    for (auto& base_base : base.bases()) {
        transform_visit(base_base, derived);
    }

    transform_base_derived(base, derived);
}

static void transform_visit(TypeDeclarationSymbol& decl)
{
    if (decl.transformed()) {
        return;
    }

    for (auto& base : decl.bases()) {
        transform_visit(base, decl);
    }

    transform_type(decl);

    decl.mark_transformed();
}

/**
 * This function traverses the hierarchy of classes/protocols/structures
 * (TypeDeclarationSymbol instances) by calling 'transform_type' for each type
 * and 'transform_base_derived' for each base-derived type pair.
 *
 * 'transform_type' can make any changes in the type symbol and its members
 * (including renaming), but cannot remove/add bases and members, or remove the
 * type symbol itself.
 *
 * 'transform_base_derived' can make changes in the derived type (with the same
 * restrictions as 'transform_type'), but cannot change the base type.
 *
 * The traversal order is from base types to derived.  More formally, for each
 * type it is guaranteed that it is visited first by a series of
 * 'transform_base_derived' calls as a derived type (if it has bases), then by
 * 'transform_type', and only after that by a series of 'transform_base_derived'
 * calls as a base type (if it has derived types).
 */
static void transform_visit()
{
    for (auto& type : Universe::get().type_definitions()) {
        transform_visit(type);
    }
}

static void set_type_mappings() noexcept
{
    for (auto&& type : Universe::get().all_declarations()) {
        for (const auto& mapping : mappings) {
            if (mapping.can_map(type)) {
                type.set_mapping(mapping);
            }
        }
    }
}

static void do_map(NonTypeSymbol& symbol)
{
    for (auto&& parameter : symbol.parameters()) {
        parameter.type().map();
    }
    symbol.return_type().map();
}

static void do_map()
{
    auto& universe = Universe::get();
    for (auto& top_level : universe.top_level()) {
        do_map(top_level);
    }
    for (auto&& decl : universe.all_declarations()) {
        if (auto* type = dynamic_cast<TypeDeclarationSymbol*>(&decl)) {
            for (auto&& member : type->members()) {
                if (!member.is_property()) {
                    do_map(member);
                }
            }
        } else if (auto* alias = dynamic_cast<TypeAliasSymbol*>(&decl)) {
            auto& target = alias->target();
            if (target.has_symbol_assigned()) {
                target.map();
            }
        }
    }
}

void apply_transforms()
{
    transform_visit();

    // Apply mappings
    set_type_mappings();
    do_map();
}

} // namespace objcgen
