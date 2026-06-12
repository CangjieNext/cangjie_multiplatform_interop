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
        if (member.is_constructor()) {
            // For 'init' methods, the cjc frontend requires the return type to be strictly
            // the declaring class, and the nullability must be nonnull.
            replace_return_instancetype(decl, member, Nullability::Nonnull);
        } else if (member.is_member_method()) {
            // For non-@ObjCInit methods, `instancetype` is mapped to the declaring class,
            // keeping the original nullability.
            const auto& original_return_type = member.return_type();
            if (is_instancetype(original_return_type)) {
                replace_return_instancetype(decl, member, original_return_type.nullability());
            }
        }
    }
}

static void resolve_static_instance_clash(NonTypeSymbol& method)
{
    method.rename(method.name() + (method.is_static() ? "Static" : "Instance"));
}

static void resolve_prop_ivar_clash(NonTypeSymbol& member)
{
    assert(member.kind() == NonTypeSymbol::Kind::Property || member.kind() == NonTypeSymbol::Kind::InstanceVariable);
    member.rename(member.name() + (member.kind() == NonTypeSymbol::Kind::InstanceVariable ? "Var" : "Prop"));
}

struct StaticInstancePair {
public:
    void add(NonTypeSymbol& type) noexcept;

    [[nodiscard]] bool clashes() const noexcept
    {
        return static_ && instance_ && static_->name() == instance_->name();
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

class PropIVarPair {
public:
    void add_prop(NonTypeSymbol& prop) noexcept;
    void add_ivar(NonTypeSymbol& ivar) noexcept;

    [[nodiscard]] bool both() const noexcept
    {
        return prop_ && ivar_;
    }

    [[nodiscard]] NonTypeSymbol* get_prop() const noexcept
    {
        return prop_;
    }

    [[nodiscard]] NonTypeSymbol* get_ivar() const noexcept
    {
        return ivar_;
    }

private:
    NonTypeSymbol* prop_;
    NonTypeSymbol* ivar_;
};

void PropIVarPair::add_prop(NonTypeSymbol& prop) noexcept
{
    assert(prop.is_property());
    assert(!prop_ && "Cannot be multiple properties with the same name");
    prop_ = &prop;
}

void PropIVarPair::add_ivar(NonTypeSymbol& ivar) noexcept
{
    assert(!ivar_ && "Cannot be multiple instance variables with the same name");
    ivar_ = &ivar;
}

void StaticInstancePair::add(NonTypeSymbol& member) noexcept
{
    if (member.is_static()) {
        assert(!static_ && "Cannot be multiple static members with the same name");
        static_ = &member;
    } else {
        assert(!instance_ && "Cannot be multiple instance members with the same name");
        instance_ = &member;
    }
}

[[nodiscard]] static bool clashes_by_name(const FileLevelSymbol& symbol1, const FileLevelSymbol* symbol2)
{
    return symbol2 && symbol2->package() == symbol1.package();
}

/*
 * If the type is tagged, and if it clashes by name with a non-tagged global
 * symbol, rename the type by adding as many "Struct"/"Union"/"Enum" suffixes as
 * needed for its uniqueness among all global symbols.  Note that the results
 * may depend on the order of declarations and on the current closure specified
 * in the configuration.
 */
static void resolve_tagged_clashes(NamedTypeSymbol& decl)
{
    auto& universe = Universe::get();
    std::string_view name = decl.name();
    if (!clashes_by_name(decl, universe.type(TypeNamespace::Primary, name)) &&
        !clashes_by_name(decl, universe.type(TypeNamespace::Protocols, name)) &&
        !clashes_by_name(decl, universe.global_non_type_symbol(name))) {
        return;
    }
    auto type_kind = decl.kind();
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
    } while (universe.type(TypeNamespace::Primary, new_name) || universe.type(TypeNamespace::Protocols, new_name) ||
        universe.type(TypeNamespace::Tagged, new_name));
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
}

template <class Params> [[nodiscard]] static bool contains_name(Params params, std::string_view name) noexcept
{
    for (const auto& param : params) {
        if (param.name() == name) {
            return true;
        }
    }
    return false;
}

template <class Params>
static unsigned rename_param(Params params, ParameterSymbol& param, const std::string& base_name, unsigned i)
{
    assert(i);
    auto new_name = base_name + std::to_string(i);
    while (contains_name(params, new_name)) {
        new_name = base_name + std::to_string(++i);
    }
    param.rename(std::move(new_name));
    return i;
}

template <class Params> static unsigned rename_param(Params params, ParameterSymbol& param, unsigned i)
{
    assert(i);
    return rename_param(params, param, param.name(), i);
}

/*
 * If some of the parameters have same names, make the names unique by adding
 * number suffixes.
 */
template <class Params> static void resolve_parameter_name_clashes(Params params)
{
    auto e = params.end();
    for (auto it1 = params.begin(); it1 != e; ++it1) {
        auto& param1 = *it1;
        const auto& name = param1.name();
        for (auto it2 = std::next(it1); it2 != e; ++it2) {
            auto& param2 = *it2;
            if (name == param2.name()) {
                rename_param(params, param2, rename_param(params, param1, 1) + 1);
            }
        }
    }
}

template <class Params> static unsigned rename_unnamed_param(Params params, ParameterSymbol& param, unsigned i)
{
    return rename_param(params, param, "_", i);
}

template <class Params> static void rename_unnamed_param(Params params, ParameterSymbol& param)
{
    auto new_name = "_";
    if (contains_name(params, new_name)) {
        rename_unnamed_param(params, param, 1);
    } else {
        param.rename(new_name);
    }
}

/*
 * If some of the parameters have no names, name them "_", possibly with number
 * suffixes if that is needed for uniqueness.
 */
template <class Params> static void resolve_unnamed_parameters(Params params)
{
    auto e = params.end();
    for (auto it1 = params.begin(); it1 != e; ++it1) {
        auto& param1 = *it1;
        if (param1.name().empty()) {
            for (auto it2 = std::next(it1); it2 != e; ++it2) {
                auto& param2 = *it2;
                if (param2.name().empty()) {
                    auto i = rename_unnamed_param(params, param2, rename_unnamed_param(params, param1, 1) + 1) + 1;
                    for (auto it3 = std::next(it2); it3 != e; ++it3) {
                        auto& param3 = *it3;
                        if (param3.name().empty()) {
                            i = rename_unnamed_param(params, param3, i);
                        }
                    }
                    return;
                }
            }
            rename_unnamed_param(params, param1);
            break;
        }
    }
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
            const auto& name = decl.name();
            if (!clashes_by_name(decl, universe.type(TypeNamespace::Primary, name)) &&
                !clashes_by_name(decl, universe.type(TypeNamespace::Tagged, name)) &&
                !clashes_by_name(decl, universe.global_non_type_symbol(name))) {
                break;
            }
            auto new_name = name;
            do {
                new_name += "Protocol";
            } while (clashes_by_name(decl, universe.type(TypeNamespace::Primary, new_name)) ||
                clashes_by_name(decl, universe.type(TypeNamespace::Tagged, new_name)) ||
                clashes_by_name(decl, universe.global_non_type_symbol(new_name)) ||
                clashes_by_name(decl, universe.type(TypeNamespace::Protocols, new_name)));
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
            resolve_tagged_clashes(decl);
            break;
        default:
            break;
    }

    auto members = decl.members();

    for (auto& member : members) {
        if (member.is_property()) {
            decl.get_getter(member).set_hidden();
            if (!member.is_readonly()) {
                decl.get_setter(member).set_hidden();
            }
        }
        if (member.is_member_method() || member.is_constructor()) {
            // In Objective-C methods, all parameters have names, but the names can be
            // non-unique.
            resolve_parameter_name_clashes(member.parameters());
        }
    }

    // Resolve static/instance clashes inside 'decl'
    std::unordered_map<std::string_view, StaticInstancePair> static_instance_map;
    for (auto& member : members) {
        if (member.is_property() || member.is_member_method()) {
            if (!member.is_hidden()) {
                static_instance_map[member.selector()].add(member);
            }
        }
    }
    for (const auto& [name, pair] : static_instance_map) {
        if (pair.clashes()) {
            auto& static_member = *pair.get_static();
            assert(static_member.name() == pair.get_instance()->name());
            resolve_static_instance_clash(static_member);
        }
    }

    // Resolve prop/ivar clashes inside 'decl'
    std::unordered_map<std::string_view, PropIVarPair> prop_ivar_map;
    for (auto& member : members) {
        switch (member.kind()) {
            case NonTypeSymbol::Kind::Property:
                prop_ivar_map[member.name()].add_prop(member);
                break;
            case NonTypeSymbol::Kind::InstanceVariable:
                prop_ivar_map[member.name()].add_ivar(member);
                break;
            default:
                break;
        }
    }
    for (const auto& [name, prop_ivar] : prop_ivar_map) {
        if (prop_ivar.both()) {
            resolve_prop_ivar_clash(*prop_ivar.get_ivar());
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

static void resolve_base_derived_name_clashes(const NonTypeSymbol& base, NonTypeSymbol& derived)
{
    assert(base.is_member_method() || base.is_property());
    assert(derived.is_member_method() || derived.is_property());
    if (base.is_static() == derived.is_static()) {
        if (base.selector() == derived.selector()) {
            if (base.is_member_method() && derived.is_member_method()) {
                // 'base' and 'derived' is a pair of non-init methods, and 'derived' overrides
                // 'base' in terms of Objective-C (same selector).  At the Cangjie side, it must
                // override as well (have the same Cangjie name).  If the names are different
                // (if 'base' was renamed at earlier transformation stages), then rename
                // 'derived' as well.
                const auto& base_name = base.name();
                if (base_name != derived.name()) {
                    derived.rename(base_name);
                }
            } else {
                // 'base' and 'derived' have the same selector.  If they are of different kind
                // (Property/MemberMethod or MemberMethod/Property) they cannot have the same
                // Cangjie name.  Also (this is mentioned in the documentation) they cannot have
                // the same Cangjie name if they are both properties.  Make 'derived' hidden in
                // both cases.
                derived.set_hidden();
            }
        }
    } else if (base.name() == derived.name()) {
        // 'base' and 'derived' have different "staticity".  Therefore, this is not an
        // override, in terms of either Objective-C or Cangjie.  But, regardless of the
        // kind (Property or MemberMethod), they must not clash by Cangjie name.  If
        // they do, rename 'derived' by adding the 'Static' or 'Instance' suffix.
        resolve_static_instance_clash(derived);
    }
}

static void transform_base_derived(const TypeDeclarationSymbol& base, TypeDeclarationSymbol& derived)
{
    auto base_members = base.members();
    auto derived_members = derived.members();
    for (auto& derived_member : derived_members) {
        if (derived_member.is_property()) {
            for (const auto& base_member : base_members) {
                if (base_member.is_property() || base_member.is_member_method()) {
                    resolve_base_derived_name_clashes(base_member, derived_member);
                }
            }
        } else if (derived_member.is_member_method()) {
            for (const auto& base_member : base_members) {
                if (base_member.is_property()) {
                    resolve_base_derived_name_clashes(base_member, derived_member);
                } else if (base_member.is_member_method()) {
                    resolve_base_derived_name_clashes(base_member, derived_member);

                    if (base_member.selector() != derived_member.selector() ||
                        base_member.is_static() != derived_member.is_static()) {
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
            }
        } else if (derived_member.is_constructor()) {
            for (const auto& base_member : base_members) {
                if (base_member.is_constructor() && base_member.selector() == derived_member.selector()) {
                    derived_member.set_override();
                }
            }
        }
    }

    for (auto& derived_member : derived_members) {
        if (derived_member.is_property() || derived_member.is_instance_variable()) {
            const auto derived_kind = derived_member.kind();
            for (const auto& base_member : base_members) {
                const auto base_kind = base_member.kind();
                if ((base_member.is_property() || base_member.is_instance_variable())
                    && base_kind != derived_kind && base_member.name() == derived_member.name()) {
                    resolve_prop_ivar_clash(derived_member);
                }
            }
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
    auto& universe = Universe::get();

    // In standalone functions, all parameter names are unique, but some of the
    // parameters can be unnamed.
    for (auto& func : universe.top_level()) {
        assert(func.is_global_function());
        resolve_unnamed_parameters(func.parameters());
    }

    for (auto& type : universe.types()) {
        if (type.is(NamedTypeSymbol::Kind::Enum)) {
            // Enumerations are not a part of the class/protocol/struct hierarchy, but they
            // may need resolving clashes with non-tagged top-level symbols.
            resolve_tagged_clashes(type);
        } else {
            auto* decl = dynamic_cast<TypeDeclarationSymbol*>(&type);
            if (decl) {
                transform_visit(*decl);
            }
        }
    }
}

static void set_type_mappings() noexcept
{
    for (auto&& type : Universe::get().types()) {
        for (const auto* mapping : mappings) {
            if (mapping->can_map(type)) {
                if (type.mapping()) {
                    std::cerr << "Warning: multiple mappings can be applied to type `" << type.name()
                              << "`.  Only the first one will be used." << std::endl;
                    break;
                }

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
    for (auto&& decl : universe.types()) {
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
