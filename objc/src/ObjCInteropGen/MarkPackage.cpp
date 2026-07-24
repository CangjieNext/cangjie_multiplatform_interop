// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "MarkPackage.h"

#include <iostream>

#include "InputFile.h"
#include "Logging.h"
#include "Package.h"
#include "SymbolVisitor.h"
#include "Universe.h"

namespace objcgen {

[[nodiscard]] static bool set_package(FileLevelSymbol& symbol)
{
    bool success = true;

    const auto& name = symbol.name();

    bool package_found = false;

    for (auto&& package : packages) {
        if (!package.filters()->apply(name)) {
            continue;
        }

        if (symbol.package()) {
            std::cerr << "Entity `" << name << "` is ambiguous between packages `" << symbol.package()->cangjie_name()
                      << "` and `" << package.cangjie_name() << '`' << std::endl;
            success = false;
            continue;
        }

        assert(symbol.output_status() == OutputStatus::Undefined);
        symbol.set_output_status(OutputStatus::Root);
        symbol.register_for_package(package);
        package_found = true;
    }

    if (!package_found && verbosity >= LogLevel::TRACE) {
        std::cerr << "Entity `" << name << "` does not match any package filter" << std::endl;
    }
    return success;
}

[[nodiscard]] static bool mark_roots()
{
    auto success = true;

    auto& universe = Universe::get();
    for (auto& member : universe.top_level()) {
        if (!set_package(member)) {
            success = false;
        }
    }

    for (auto&& type : universe.types()) {
        // Omit types having no definition in source files (those are built-ins like
        // `id`).
        if (!type.defining_file()) {
            continue;
        }

        if (!set_package(type)) {
            success = false;
        }
    }

    return success;
}

class SymbolReferenceCollector final : public SymbolVisitor {

public:
    [[nodiscard]] explicit SymbolReferenceCollector(FileLevelSymbol& symbol) noexcept
        : SymbolVisitor(true), symbol_(symbol)
    {
    }

    void visit()
    {
        SymbolVisitor::visit(symbol_);
    }

private:
    FileLevelSymbol& symbol_;

    void visit_impl(const FileLevelSymbol& symbol) override;
};

void SymbolReferenceCollector::visit_impl(const FileLevelSymbol& symbol)
{
    if (&symbol != &symbol_ // Self-reference
        && symbol.defining_file()) {
        if (symbol_.add_reference(const_cast<FileLevelSymbol&>(symbol)) && verbosity >= LogLevel::TRACE) {
            std::cerr << "Entity `" << symbol_.name() << "` references `" << symbol.name() << "`\n";
        }
    }
}

class ScopeBuilderStatus final {
    bool success_ = true;
    bool changed_ = false;

public:
    [[nodiscard]] bool success() const noexcept
    {
        return success_;
    }

    [[nodiscard]] bool error() const noexcept
    {
        return !success();
    }

    [[nodiscard]] bool changed() const noexcept
    {
        return changed_;
    }

    void mark_error() noexcept
    {
        success_ = false;
    }

    void mark_changed() noexcept
    {
        changed_ = true;
    }
};

static void add_all_symbol_references()
{
    for (const auto& input_file : inputs) {
        for (auto& symbol : input_file) {
            assert(symbol.defining_file());
            SymbolReferenceCollector(symbol).visit();
        }
    }
}

static void set_root_reference_level() noexcept
{
    for (const auto& input_file : inputs) {
        for (auto& symbol : input_file) {
            if (symbol.output_status() == OutputStatus::Root) {
                symbol.set_reference_level(0);
            }
        }
    }
}

static void symbol_references_to_packages_pass(ScopeBuilderStatus& status, FileLevelSymbol& symbol, bool roots_only)
{
    assert(symbol.defining_file());

    if (symbol.output_status() != (roots_only ? OutputStatus::Root : OutputStatus::Referenced)) {
        return;
    }

    auto* package = symbol.package();
    if (package) {
        for (auto* reference : symbol.references_symbols()) {
            assert(reference);
            switch (reference->output_status()) {
                case OutputStatus::Undefined:
                    assert(!reference->package());
                    reference->set_output_status(OutputStatus::Referenced);
                    reference->add_referencing_package(*package);
                    if (reference->reference_level() <= Config::closure_depth()) {
                        reference->register_for_package(*package);
                    }
                    status.mark_changed();
                    break;
                case OutputStatus::Referenced:
                case OutputStatus::ReferencedMarked: {
                    const auto* reference_package = reference->package();
                    if (reference_package && reference_package != package) {
                        // It makes sense to build graph of dependencies between packages and resolve
                        // the most common cases by selecting the closest common dependency package.
                        reference->set_output_status(OutputStatus::MultiReferenced);
                        reference->add_referencing_package(*package);
                        status.mark_error();
                    }
                    break;
                }
                default:
                    assert(reference->package());
                    break;
            }
        }
    }
    if (!roots_only) {
        assert(symbol.output_status() == OutputStatus::Referenced);
        symbol.set_output_status(OutputStatus::ReferencedMarked);
    }
}

[[nodiscard]] static ScopeBuilderStatus symbol_references_to_packages_pass(bool roots_only)
{
    ScopeBuilderStatus status;

    for (const auto& input_file : inputs) {
        for (auto& symbol : input_file) {
            symbol_references_to_packages_pass(status, symbol, roots_only);
        }
    }

    return status;
}

[[nodiscard]] static bool symbol_references_to_packages()
{
    set_root_reference_level();

    auto status = symbol_references_to_packages_pass(true);
    auto error = status.error();
    while (status.changed()) {
        status = symbol_references_to_packages_pass(false);
        if (status.error()) {
            error = true;
        }
    }

    for (const auto& input_file : inputs) {
        for (auto& symbol : input_file) {
            switch (symbol.output_status()) {
                case OutputStatus::Undefined:
                    if (verbosity >= LogLevel::DEBUG) {
                        std::cerr << "Entity `" << symbol.name() << "` from `" << input_file.path().u8string()
                                  << "` is not used" << std::endl;
                    }
                    break;
                case OutputStatus::Referenced:
                case OutputStatus::ReferencedMarked:
                    if (verbosity >= LogLevel::TRACE) {
                        std::cerr << "Entity `" << symbol.name() << "` from `" << input_file.path().u8string();
                        const auto* package = symbol.package();
                        if (package) {
                            assert(symbol.package_file());
                            std::cerr << "` is only used from `" << package->cangjie_name() << "` package, assigning `"
                                      << symbol.package_file()->output_path().u8string() << '`';
                        } else {
                            std::cerr << "` was filtered out by --closure-depth";
                        }
                        std::cerr << std::endl;
                    }
                    break;
                case OutputStatus::MultiReferenced:
                    std::cerr << "Entity `" << symbol.name() << "` from `" << input_file.path().u8string()
                              << "` is ambiguous between " << symbol.number_of_referencing_packages() << " packages";
                    symbol.print_referencing_packages_info();
                    break;
                default:
                    break;
            }
        }
    }

    return !error;
}

static void register_symbols_in_declaration_order()
{
    for (const auto& input_file : inputs) {
        for (auto& symbol : input_file) {
            assert(symbol.defining_file());

            if (auto* package_file = symbol.package_file()) {
                package_file->add_symbol(symbol);
            }
        }
    }
}

/** Given the N-dimensional VArray, get the type of its element */
[[nodiscard]] static const Type& get_element_type(const Type& varray) noexcept
{
    assert(varray.kind() == Type::Kind::VArray);
    const auto& element_type = varray.varray_element_type();
    return element_type.kind() == Type::Kind::VArray ? get_element_type(element_type) : element_type;
}

/**
 * For each function parameter, if the type of the parameter is array, convert
 * it to pointer to its element
 */
static void decay_parameter_types(NonTypeSymbol& function)
{
    for (auto& parameter : function.parameters()) {
        auto type = parameter.type().canonical_type();
        if (type.kind() == Type::Kind::VArray) {
            parameter.set_type(Type(Universe::get().pointer(), {get_element_type(type)}));
        }
    }
}

/**
 * For each function parameter, if the type of the parameter is array, convert
 * it to pointer to its element
 */
static void decay_parameter_types()
{
    auto& universe = Universe::get();
    for (auto& top_level : universe.top_level()) {
        decay_parameter_types(top_level);
    }
    for (auto& type : universe.types()) {
        auto* decl = dynamic_cast<TypeDeclarationSymbol*>(&type);
        if (decl) {
            for (auto& member : decl->members()) {
                decay_parameter_types(member);
            }
        }
    }
}

bool mark_package()
{
    decay_parameter_types();

    if (!mark_roots()) {
        return false;
    }

    add_all_symbol_references();

    if (!symbol_references_to_packages()) {
        return false;
    }

    register_symbols_in_declaration_order();

    return true;
}

} // namespace objcgen
