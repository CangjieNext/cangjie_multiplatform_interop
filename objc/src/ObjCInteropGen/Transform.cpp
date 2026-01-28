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

static void append_name(Symbol& symbol, std::string_view suffix)
{
    symbol.rename(std::string(symbol.name()).append(suffix));
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
        assert(dynamic_cast<TypeDeclarationSymbol*>(&super_superclass));
        resolve_static_instance_clashes(subclass, static_cast<TypeDeclarationSymbol&>(super_superclass));
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
                                append_name(submember, "Static");
                            }
                        } else if (supermember.is_static()) {
                            append_name(submember, "Instance");
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

    bool both() const noexcept
    {
        return static_ && instance_;
    }

    NonTypeSymbol* get_static() const noexcept
    {
        return static_;
    }

    NonTypeSymbol* get_instance() const noexcept
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
        assert(dynamic_cast<TypeDeclarationSymbol*>(&supertype));
        resolve_static_instance_clashes(type, static_cast<TypeDeclarationSymbol&>(supertype));
    }

    // Resolve conflicts inside the class.
    std::unordered_map<std::string_view, StaticInstancePair> map;
    for (auto& member : type.members()) {
        if (member.kind() == NonTypeSymbol::Kind::MemberMethod) {
            map[member.selector()].add(member);
        }
    }
    for (const auto& [name, methods] : map) {
        if (methods.both() && methods.get_static()->name() == methods.get_instance()->name()) {
            append_name(*methods.get_static(), "Static");
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
                    assert(member_j.parameter_count() == member_i.parameter_count());
                    type.member_remove(j);
                    --num_members;
                } else {
                    ++j;
                }
            }
        }
    }
}

void do_rename()
{
    auto type_definitions = Universe::type_definitions();

    for (auto&& type : type_definitions) {
        if (type.is(NamedTypeSymbol::Kind::Protocol)) {
            auto name = std::string(type.name());
            if (universe.type(TypeNamespace::Primary, name)) {
                auto new_name = name + "Protocol";
                if (verbosity >= LogLevel::INFO) {
                    std::cerr << "Renaming clashing protocol `" << name << "` to `" << new_name << "`" << std::endl;
                }
                type.rename(new_name);
            }
        }

        remove_duplicates(type);

        for (auto&& member : type.members()) {
            if (member.name().find(':') != std::string_view::npos) {
                std::string new_name;
                auto upcase = false;
                for (auto c : member.name()) {
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
}

void set_type_mappings()
{
    for (auto&& type : Universe::all_declarations()) {
        for (auto&& mapping_ptr : mappings) {
            auto& mapping = *mapping_ptr;
            if (mapping.can_map(&type)) {
                type.set_mapping(&mapping);
            }
        }
    }
}

static void do_type_map()
{
    for (auto&& decl : Universe::all_declarations()) {
        if (auto* type = dynamic_cast<TypeDeclarationSymbol*>(&decl)) {
            for (auto&& member : type->members()) {
                if (member.is_property()) {
                    continue;
                }
                for (auto&& parameter : member.parameters()) {
                    parameter.set_type(parameter.type()->map());
                }
                auto return_type = member.return_type()->map();
                assert(return_type);
                if (return_type->is_instancetype()) {
                    return_type = &decl;
                }
                member.set_return_type(return_type);
            }
        } else if (auto* alias = dynamic_cast<TypeAliasSymbol*>(&decl)) {
            auto* target = alias->target();
            // `instancetype` is a special type which has no explicit declaration located in
            // a file, so it does not set a target.
            if (target) {
                alias->set_target(target->map());
            }
        }
    }
}

void apply_transforms()
{
    do_rename();
    set_type_mappings();
    do_type_map();
}
