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

static void fix_override_return_types(TypeDeclarationSymbol& type) noexcept;

static void fix_override_return_types(TypeDeclarationSymbol& subclass, TypeDeclarationSymbol& superclass) noexcept
{
    // Recursively process `superclass` and its ancestor hierarchy.
    fix_override_return_types(superclass);

    // Asserting that `superclass` is an ancestor of `subclass`, this loop
    // recursively resolves clashes between `subclass` and each of the ancestors of
    // `superclass`, starting from the root(s), sequentially.
    for (auto& super_superclass : superclass.bases()) {
        fix_override_return_types(subclass, super_superclass);
    }

    // This loop resolves clashes between `subclass` and `superclass`, where the
    // latter is asserted to be one of the ancestors of the former.
    for (auto& submember : subclass.members()) {
        if (submember.kind() == NonTypeSymbol::Kind::MemberMethod) {
            for (auto& supermember : superclass.members()) {
                if (supermember.kind() == NonTypeSymbol::Kind::MemberMethod &&
                    supermember.is_static() == submember.is_static() &&
                    supermember.selector() == submember.selector()) {
                    submember.set_override();
                    const auto& supermember_type = supermember.return_type();
                    auto& submember_type = submember.return_type();
                    if (supermember_type.nullability() != Nullability::Nonnull) {
                        if (submember_type.nullability() == Nullability::Nonnull) {
                            submember_type.set_nullability(Nullability::Nullable);
                        }

                        // Both are Option.  Must be the same type.
                        submember.set_return_type(supermember_type);
                    } else {
                        if (submember_type.nullability() != Nullability::Nonnull) {
                            submember_type.set_nullability(Nullability::Nonnull);
                        }

                        // Both are non-Option.  Either they must be the same or the overridden must be
                        // the base of the overrider.
                        const auto* submember_type_decl =
                            dynamic_cast<const TypeDeclarationSymbol*>(&submember_type.symbol());
                        if (submember_type_decl) {
                            const auto* supermember_type_decl =
                                dynamic_cast<const TypeDeclarationSymbol*>(&supermember_type.symbol());
                            if (supermember_type_decl) {
                                if (!is_base_of(*supermember_type_decl, *submember_type_decl)) {
                                    submember.set_return_type(supermember_type);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

static void fix_override_return_types(TypeDeclarationSymbol& type) noexcept
{
    if (type.are_override_return_clashes_resolved()) {
        return;
    }

    // Recursively call this function for all ancestors, then resolve conflicts
    // between this class and each of the ancestors.
    for (auto& supertype : type.bases()) {
        fix_override_return_types(type, supertype);
    }

    type.mark_override_return_clashes_resolved();
}

/**
 * 1) In Cangjie, Option is not covariant.  If the overridden and overrider have
 * different nullabilities, fix the result type of the overrider.
 *
 * 3) In Cangjie, Option is not covariant.  If both the overridden and overrider
 * are nullable, ensure that the overrider does not change the result type.
 *
 * 2) In Objective-C, contravariant return types are allowed.  That will not
 * compile in Cangjie. Fix the result type of the overrider accordingly.
 */
static void fix_override_return_types()
{
    for (auto& type : Universe::get().type_definitions()) {
        fix_override_return_types(type);
    }
}

static void resolve_static_instance_clash(Symbol& symbol, bool is_static)
{
    symbol.rename(std::string(symbol.name()).append(is_static ? "Static" : "Instance"));
}

static void resolve_static_instance_clash(TypeDeclarationSymbol& decl, NonTypeSymbol& method, bool is_static)
{
    assert(method.kind() == NonTypeSymbol::Kind::MemberMethod);
    resolve_static_instance_clash(method, is_static);

    // This method can be the getter of a property.  Rename the property too.
    for (auto& member : decl.members()) {
        if (member.kind() == NonTypeSymbol::Kind::Property && member.is_static() == is_static &&
            member.getter() == method.selector()) {
            resolve_static_instance_clash(member, is_static);
            break;
        }
    }
}

static void resolve_static_instance_clashes(TypeDeclarationSymbol& type);

static void resolve_static_instance_clashes(TypeDeclarationSymbol& subclass, TypeDeclarationSymbol& superclass)
{
    // Recursively process `superclass` and its ancestor hierarchy.
    resolve_static_instance_clashes(superclass);

    // Asserting that `superclass` is an ancestor of `subclass`, this loop
    // recursively resolves clashes between `subclass` and each of the ancestors of
    // `superclass`, starting from the root(s), sequentially.
    for (auto& super_superclass : superclass.bases()) {
        resolve_static_instance_clashes(subclass, super_superclass);
    }

    // This loop resolves clashes between members of `subclass` and `superclass`,
    // where the latter is asserted to be one of the ancestors of the former.
    for (auto& submember : subclass.members()) {
        if (submember.kind() == NonTypeSymbol::Kind::MemberMethod) {
            for (const auto& supermember : superclass.members()) {
                if (supermember.kind() == NonTypeSymbol::Kind::MemberMethod &&
                    supermember.selector() == submember.selector()) {
                    auto supername = supermember.name();
                    if (supername == submember.name()) {
                        if (submember.is_static()) {
                            if (!supermember.is_static()) {
                                resolve_static_instance_clash(subclass, submember, true);
                            }
                        } else if (supermember.is_static()) {
                            resolve_static_instance_clash(subclass, submember, false);
                        }
                    } else {
                        // The supermember could be already renamed.  In Objective-C methods are not
                        // overloaded.  Therefore, if the two methods have the same selector, that means
                        // one of them overrides the other, and they must have the same name.
                        submember.rename(supername);
                    }
                }
            }
        }
    }
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

    [[nodiscard]] NonTypeSymbol* get_instance() const noexcept
    {
        return instance_;
    }

private:
    NonTypeSymbol* static_;
    NonTypeSymbol* instance_;
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

/**
 * Resolve static/instance clashes in the `type` class hierarchy.
 *
 * That is, if any class in the `type` hierarchy contains static or instance
 * methods conflicting by name with, correspondingly, instance or static methods
 * of this very class or one of its bases (direct or indirect), the conflicts
 * are resolved by appending the "Static" or "Instance" suffix to the method
 * names.
 *
 * Each class/protocol is checked for clashes with each of its ancestors, from
 * top to bottom sequentially.  If a clash is found, the "Static" or "Instance"
 * suffix is appended to the descendant's conflicting method name.  Then clashes
 * are resolved inside the class/protocol itself.  The static method is renamed
 * in this case.
 *
 * Such a procedure is performed for each vertex of the directed acyclic graph
 * of `type` and all its ancestors (classes and protocols).  The graph is
 * traversed from the root(s) to `type`.  After processing, each vertex is
 * marked as resolved. It is not re-processed again in this and subsequent calls
 * of the function.
 */
static void resolve_static_instance_clashes(TypeDeclarationSymbol& type)
{
    if (type.are_static_instance_clashes_resolved()) {
        return;
    }

    // Recursively call this function for all ancestors, then resolve conflicts
    // between this class and each of the ancestors.
    for (auto& supertype : type.bases()) {
        resolve_static_instance_clashes(type, supertype);
    }

    // Resolve conflicts inside the class.
    std::unordered_map<std::string_view, StaticInstancePair> map;
    for (auto& member : type.members()) {
        if (member.kind() == NonTypeSymbol::Kind::MemberMethod) {
            map[member.selector()].add(member);
        }
    }
    for (const auto& [name, methods] : map) {
        if (methods.both()) {
            auto& static_method = *methods.get_static();
            if (static_method.name() == methods.get_instance()->name()) {
                resolve_static_instance_clash(type, static_method, true);
            }
        }
    }

    map.clear();
    for (auto& member : type.members()) {
        if (member.kind() == NonTypeSymbol::Kind::Property) {
            map[member.selector()].add(member);
        }
    }
    for (const auto& [name, props] : map) {
        if (props.both()) {
            auto& static_prop = *props.get_static();
            if (static_prop.name() == props.get_instance()->name()) {
                resolve_static_instance_clash(static_prop, true);
            }
        }
    }

    type.mark_static_instance_clashes_resolved();
}

/**
 * Removes duplicate method declarations. In Objective-C, it is allowed to
 * declare a method more than once.  In Cangjie, it is not.
 */
static void remove_duplicates(TypeDeclarationSymbol& type)
{
    auto num_members = type.member_count();
    for (size_t i = 0; i < num_members; ++i) {
        const auto& member_i = type.member(i);
        if (member_i.kind() == NonTypeSymbol::Kind::MemberMethod) {
            for (size_t j = i + 1; j < num_members;) {
                const auto& member_j = type.member(j);
                if (member_j.kind() == NonTypeSymbol::Kind::MemberMethod &&
                    member_j.is_static() == member_i.is_static() && member_j.name() == member_i.name()) {
                    assert(member_j.parameters().size() == member_i.parameters().size());
                    type.member_remove(j);
                    --num_members;
                } else {
                    ++j;
                }
            }
        }
    }
}

static void do_rename()
{
    auto& universe = Universe::get();
    auto type_definitions = universe.type_definitions();

    for (auto&& type : type_definitions) {
        if (type.is(NamedTypeSymbol::Kind::Protocol)) {
            for (;;) {
                const auto& name = type.name();
                bool clashing = false;
                for (uint8_t ns = 0; ns < TYPE_NAMESPACE_COUNT; ++ns) {
                    auto namespaze = static_cast<TypeNamespace>(ns);
                    if (namespaze != TypeNamespace::Protocols) {
                        const auto* non_protocol_type = universe.type(namespaze, name);
                        if (non_protocol_type && non_protocol_type->package() == type.package()) {
                            auto new_name = name + "Protocol";
                            if (verbosity >= LogLevel::INFO) {
                                std::cerr << "Renaming clashing protocol `" << name << "` to `" << new_name << '`'
                                          << std::endl;
                            }
                            type.rename(new_name);
                            clashing = true;
                            break;
                        }
                    }
                }
                if (!clashing) {
                    break;
                }
            }
        }

        remove_duplicates(type);

        for (auto&& member : type.members()) {
            const auto& name = member.name();
            if (name.find(':') != std::string_view::npos) {

                std::string new_name;
                auto upcase = false;
                for (auto c : name) {
                    if (c == ':') {
                        upcase = true;
                        continue;
                    }

                    if (upcase) {
                        c = static_cast<char>(std::toupper(c));
                        upcase = false;
                    }

                    new_name += c;
                }

                member.rename(new_name);
            }
        }
    }

    for (auto& type : type_definitions) {
        resolve_static_instance_clashes(type);
    }

    // In Objective-C, a global function can share the same name with a
    // structure/class/protocol.  In Cangjie, it cannot.  Resolve the conflict by
    // adding the `Func` suffix to the name of the global function.
    for (auto& top_level : universe.top_level()) {
        if (top_level.is_global_function()) {
            const auto& name = top_level.name();
            const auto* type = universe.type(name);
            if (type && type->package() == top_level.package()) {
                top_level.rename(name + "Func");
            }
        }
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
            if (target) {
                target.map();
            }
        }
    }
}

void apply_transforms()
{
    // 1) Resolve contravariance and nullability clashes in return types of override
    // methods.
    // 2) Set ModifierOverride where needed.
    fix_override_return_types();

    // 1) Resolve class/protocol name clashes.
    // 2) Remove duplicate member declarations.
    // 3) Rename methods in accordance with their Objective-C selectors.
    // 4) Resolve static/instance name clashes.
    // 5) Resolve global function - class/protocol/structure name clashes.
    do_rename();

    // Apply mappings
    set_type_mappings();
    do_map();
}

} // namespace objcgen
