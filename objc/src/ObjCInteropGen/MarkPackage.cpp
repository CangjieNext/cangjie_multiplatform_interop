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
#include "SingleDeclarationSymbolVisitor.h"
#include "Universe.h"

namespace objcgen {

static PackageFile* input_to_output(Package* package, const InputFile* input)
{
    const auto file_name = input->path().stem().u8string();
    if (auto* file = (*package)[file_name]) {
        return file;
    }
    return new PackageFile(file_name, package);
}

static PackageFile* input_to_output(Package* package, const FileLevelSymbol* symbol)
{
    auto* input_file = symbol->defining_file();
    assert(input_file);
    return input_to_output(package, input_file);
}

static bool check_symbol(FileLevelSymbol* symbol)
{
    assert(symbol);
    if (const auto* named = dynamic_cast<NamedTypeSymbol*>(symbol)) {
        if (named->is(NamedTypeSymbol::Kind::SourcePrimitive))
            return false;
        if (named->is(NamedTypeSymbol::Kind::TargetPrimitive))
            return false;
    }
    return symbol->is_file_level();
}

static bool mark_roots()
{
    auto success = true;

    for (auto&& type : Universe::get().all_declarations()) {
        // Omit primitive types, as well as types having no definition in source files
        // (those are built-ins like `id`).
        if (type.is(NamedTypeSymbol::Kind::SourcePrimitive) || type.is(NamedTypeSymbol::Kind::TargetPrimitive) ||
            !type.defining_file()) {
            continue;
        }

        const auto type_name = type.name();

        auto package_found = false;

        for (auto&& package : packages) {
            if (!package.filters()->apply(type_name)) {
                continue;
            }

            if (type.package()) {
                std::cerr << "Entity `" << type_name << "` is ambiguous between packages `"
                          << type.package()->cangjie_name() << "` and `" << package.cangjie_name() << "`" << std::endl;
                success = false;
                continue;
            }

            type.set_package_file(input_to_output(&package, &type));
            package_found = true;
        }

        if (!package_found && verbosity >= LogLevel::TRACE) {
            std::cerr << "Entity `" << type_name << "` does not match any package filter" << std::endl;
        }
    }

    return success;
}

class SymbolReferenceCollector final : public SingleDeclarationSymbolVisitor {

public:
    [[nodiscard]] explicit SymbolReferenceCollector() : SingleDeclarationSymbolVisitor(true)
    {
    }

    SymbolReferenceCollector(const SymbolReferenceCollector& other) = delete;

    SymbolReferenceCollector(SymbolReferenceCollector&& other) noexcept = delete;

    SymbolReferenceCollector& operator=(const SymbolReferenceCollector& other) = delete;

    SymbolReferenceCollector& operator=(SymbolReferenceCollector&& other) noexcept = delete;

    [[nodiscard]] const std::vector<FileLevelSymbol*>& symbols() const
    {
        return symbols_;
    }

    void clear()
    {
        symbols_.clear();
    }

protected:
    void visit_impl(FileLevelSymbol* owner, FileLevelSymbol* value, SymbolProperty, bool) override
    {
        assert(value);
        if (!owner) {
            // Skip the root type of this visit session to avoid self-referencing of each type.
            return;
        }
        if (value->is_file_level()) {
            symbols_.push_back(value);
        }
    }

private:
    std::vector<FileLevelSymbol*> symbols_;
};

static void add_all_symbol_references()
{
    SymbolReferenceCollector collector;

    for (const auto* input_directory : inputs) {
        for (const auto* input_file : *input_directory) {
            for (auto* symbol : *input_file) {
                assert(check_symbol(symbol));

                collector.visit(symbol);

                for (auto* used : collector.symbols()) {
                    if (used == symbol) {
                        // Self-reference
                        continue;
                    }

                    if (verbosity >= LogLevel::TRACE) {
                        std::cerr << "Entity `" << symbol->name() << "` references `" << used->name() << "`"
                                  << std::endl;
                    }

                    used->add_reference(symbol);
                }

                collector.clear();
            }
        }
    }
}

class ScopeBuilderStatus final {
    bool success_ = true;
    bool changed_ = false;
    bool has_undecided_ = false;

public:
    [[nodiscard]] bool success() const
    {
        return success_;
    }

    [[nodiscard]] bool error() const
    {
        return !success();
    }

    [[nodiscard]] bool changed() const
    {
        return changed_;
    }

    [[nodiscard]] bool has_undecided() const
    {
        return has_undecided_;
    }

    void mark_error()
    {
        success_ = false;
    }

    void mark_changed()
    {
        changed_ = true;
    }

    void mark_has_undecided()
    {
        has_undecided_ = true;
    }
};

static void symbol_references_to_packages_pass(ScopeBuilderStatus& status,
    std::unordered_map<Package*, std::uint64_t>& package_counts, const InputDirectory& input_directory,
    const InputFile& input_file, FileLevelSymbol& symbol, bool aggressive)
{
    assert(check_symbol(&symbol));

    if (symbol.package_file() || symbol.no_output_file()) {
        return;
    }

    package_counts.clear();
    std::uint64_t no_package_count = 0;

    for (const auto* reference : symbol.references_symbols()) {
        if (auto* package = reference->package()) {
            if (auto it = package_counts.find(package); it != package_counts.end()) {
                it->second++;
            } else {
                package_counts.emplace(package, 1);
            }
        } else if (!reference->no_output_file()) {
            no_package_count++;
        }
    }

    if (package_counts.empty() && (no_package_count == 0 || aggressive)) {
        if (verbosity >= LogLevel::DEBUG) {
            std::cerr << "Entity `" << symbol.name() << "` from `" << input_file.path().u8string() << "` is not used"
                      << std::endl;
        }
        symbol.mark_no_output_file();
        status.mark_changed();
        return;
    }

    if (package_counts.size() == 1 && (no_package_count == 0 || aggressive)) {
        auto* package = package_counts.begin()->first;
        assert(package);
        auto* package_file = input_to_output(package, &input_file);
        assert(package_file);
        if (verbosity >= LogLevel::TRACE) {
            std::cerr << "Entity `" << symbol.name() << "` from `" << input_file.path().u8string()
                      << "` is only used from `" << package->cangjie_name() << "` package, assigning `"
                      << package_file->output_path().u8string() << "`" << std::endl;
        }
        symbol.set_package_file(package_file);
        status.mark_changed();
        return;
    }

    if (package_counts.size() > 1) {
        // TODO: build graph of dependencies between packages and resolve the most common cases
        //       by selecting the closest common dependency package
        std::cerr << "Entity `" << symbol.name() << "` from `" << input_file.path().u8string()
                  << "` is ambiguous between " << package_counts.size() << " packages";
        if (verbosity == LogLevel::WARNING) {
            std::cerr << ". Specify -v for more detailed information" << std::endl;
        } else {
            std::cerr << ":" << std::endl;
            for (const auto [package, _] : package_counts) {
                std::cerr << "* " << package->cangjie_name() << std::endl;
            }
        }
        status.mark_error();
        return;
    }

    assert(no_package_count != 0);
    status.mark_has_undecided();
    if (verbosity >= LogLevel::TRACE) {
        std::cerr << "Entity `" << symbol.name() << "` is undecided:" << std::endl;
        for (const auto* reference : symbol.references_symbols()) {
            std::cerr << "* " << reference->name();
            if (const auto* package = reference->package()) {
                std::cerr << " from package `" << package->cangjie_name() << "`";
            } else if (reference->no_output_file()) {
                std::cerr << " (no output file)";
            } else {
                std::cerr << " (no package)";
            }
            std::cerr << std::endl;
        }
    }
}

static ScopeBuilderStatus symbol_references_to_packages_pass(bool aggressive)
{
    ScopeBuilderStatus status;

    std::unordered_map<Package*, std::uint64_t> package_counts;

    for (const auto* input_directory : inputs) {
        for (const auto* input_file : *input_directory) {
            for (auto* symbol : *input_file) {
                symbol_references_to_packages_pass(
                    status, package_counts, *input_directory, *input_file, *symbol, aggressive);
            }
        }
    }

    return status;
}

static bool symbol_references_to_packages()
{
    while (true) {
        auto status = symbol_references_to_packages_pass(false);
        if (status.error()) {
            return false;
        }

        if (status.changed()) {
            continue;
        }

        if (!status.has_undecided()) {
            return true;
        }

        status = symbol_references_to_packages_pass(true);
        if (status.error()) {
            return false;
        }

        if (status.changed()) {
            continue;
        }

        return false;
    }
}

static void register_symbols_in_declaration_order()
{
    for (const auto* input_directory : inputs) {
        for (const auto* input_file : *input_directory) {
            for (auto* symbol : *input_file) {
                assert(check_symbol(symbol));

                if (auto* package_file = symbol->package_file()) {
                    package_file->add_symbol(symbol);

                    auto* edge_to = package_file->package();
                    assert(edge_to);
                    for (const auto* reference : symbol->references_symbols()) {
                        if (auto* edge_from = reference->package()) {
                            assert(!reference->no_output_file());
                            if (edge_from != edge_to) {
                                edge_from->add_dependency_edge(edge_to);
                            }
                        }
                    }
                }
            }
        }
    }
}

/** Given the N-dimensional VArray, get the type of its element */
static TypeLikeSymbol& get_element_type(const VArraySymbol& varray) noexcept
{
    const auto* subvarray = dynamic_cast<const VArraySymbol*>(varray.element_type_);
    return subvarray ? get_element_type(*subvarray) : *varray.element_type_;
}

/**
 * For each function parameter, if the type of the parameter is array, convert
 * it to pointer to its element
 */
static void decay_parameter_types()
{
    for (auto& type : Universe::get().all_declarations()) {
        auto* decl = dynamic_cast<TypeDeclarationSymbol*>(&type);
        if (decl) {
            for (auto& member : decl->members()) {
                if (member.is_method()) {
                    for (auto& parameter : member.parameters()) {
                        auto* parameter_type = parameter.type();
                        const auto* varray = dynamic_cast<const VArraySymbol*>(&parameter_type->canonical_type());
                        if (varray) {
                            parameter.set_type(&pointer(get_element_type(*varray)));
                        }
                    }
                }
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
