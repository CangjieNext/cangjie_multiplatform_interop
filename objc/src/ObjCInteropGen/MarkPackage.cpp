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
#include "Universe.h"

namespace objcgen {

namespace {
class SymbolDepthPrinter {
public:
    [[nodiscard]] explicit SymbolDepthPrinter(const FileLevelSymbol& symbol) : value_(symbol.reference_level())
    {
    }

    SymbolDepthPrinter(const SymbolDepthPrinter& other) = delete;

    SymbolDepthPrinter(SymbolDepthPrinter&& other) noexcept = delete;

    SymbolDepthPrinter& operator=(const SymbolDepthPrinter& other) = delete;

    SymbolDepthPrinter& operator=(SymbolDepthPrinter&& other) noexcept = delete;

    friend std::ostream& operator<<(std::ostream& os, const SymbolDepthPrinter& obj)
    {
        if (obj.value_ == UNLIMITED_CLOSURE_DEPTH) {
            return os << "no computed depth";
        }
        return os << "depth " << obj.value_;
    }

private:
    ClosureDepthType const value_;
};

enum class ReferenceKind { None, Likely, True };
} // namespace

[[nodiscard]] static bool is_root(const Package& package, const FileLevelSymbol& symbol) noexcept
{
    const auto& name = symbol.objc_name();

    // When `root-filters` is present, only those matches become closure roots.
    if (const auto* filter = package.root_filters()) {
        return filter->apply(name);
    }

    // Otherwise, other filter properties act as roots.
    if (const auto* filter = package.filters(); filter && filter->apply(name)) {
        return true;
    }

    return false;
}

[[nodiscard]] static bool matches_any_filter(const Package& package, const FileLevelSymbol& symbol) noexcept
{
    const auto& name = symbol.objc_name();

    if (const auto* filter = package.root_filters(); filter && filter->apply(name)) {
        return true;
    }

    if (const auto* filter = package.filters(); filter && filter->apply(name)) {
        return true;
    }

    return false;
}

static void set_package(FileLevelSymbol& symbol)
{
    // If not set, the output Cangjie package will be controlled by the packages that reference this symbol.
    bool matches_any_filters = false;

    for (auto&& package : packages) {
        if (is_root(package, symbol) && symbol.output_status() != OutputStatus::Root) {
            symbol.mark_root();
        }

        if (matches_any_filter(package, symbol)) {
            // The output Cangjie package is known.
            matches_any_filters = true;
            const auto changed = symbol.add_candidate_package(package);
            assert(changed);
        }
    }

    symbol.set_matches_any_filters(matches_any_filters);
}

[[nodiscard]] static ReferenceKind get_reference_kind(const FileLevelSymbol& symbol, const Package& package)
{
    if (symbol.output_status() == OutputStatus::Undefined) {
        return ReferenceKind::None;
    }

    if (const auto* package_from = symbol.package()) {
        return package_from == &package ? ReferenceKind::True : ReferenceKind::None;
    }

    for (const auto* package_from : symbol.candidate_packages()) {
        if (package_from == &package) {
            return ReferenceKind::Likely;
        }
    }

    return ReferenceKind::None;
}

static void collect_ambiguity_reasons(
    const Package& package, const FileLevelSymbol& symbol, std::vector<std::string>& reasons) noexcept
{
    const auto& name = symbol.objc_name();

    if (const auto* filter = package.root_filters(); filter && filter->apply(name)) {
        reasons.emplace_back("matches `root-filters`");
    }

    if (const auto* filter = package.filters(); filter && filter->apply(name)) {
        reasons.emplace_back("matches `filters`");
    }

    if (verbosity > LogLevel::WARNING) {
        for (const auto* reference : symbol.referenced_by_symbols()) {
            reasons.emplace_back("referenced by `" + reference->name() + '`');
        }
    } else {
        for (const auto& input_file : inputs) {
            for (const auto& symbol_from : input_file) {
                const auto reference_kind = get_reference_kind(symbol_from, package);
                if (reference_kind == ReferenceKind::None) {
                    continue;
                }

                std::string likely = reference_kind == ReferenceKind::Likely ? "likely " : "";

                for (const auto* reference : symbol_from.references_symbols()) {
                    if (reference == &symbol) {
                        reasons.emplace_back(likely + "referenced by `" + symbol_from.name() + '`');
                    }
                }
            }
        }
    }
}

static void register_symbol_for_package(FileLevelSymbol& symbol, bool& success)
{
    assert(symbol.defining_file());

    const auto output_status = symbol.output_status();
    const auto& candidate_packages = symbol.candidate_packages();

    if (candidate_packages.empty()) {
        if (output_status == OutputStatus::Undefined) {
            return;
        }

        assert(false);
        success = false;
        return;
    }

    const bool within_closure_depth = symbol.reference_level() <= Config::closure_depth();
    const bool referenced = output_status == OutputStatus::Referenced;
    const bool root_or_referenced = referenced || output_status == OutputStatus::Root;

    if (candidate_packages.size() == 1) {
        auto& candidate_package = **candidate_packages.begin();
        if (within_closure_depth && root_or_referenced) {
            symbol.register_for_package(candidate_package);
        }
        return;
    }

    // It makes sense to build graph of dependencies between packages and resolve
    // the most common cases by selecting the closest common dependency package.

    if (within_closure_depth && root_or_referenced) {
        success = false;
    }
}

static void diagnose_symbol(const FileLevelSymbol& symbol)
{
    assert(symbol.defining_file());

    const auto& input_file = *symbol.defining_file();
    const auto output_status = symbol.output_status();
    const auto& candidate_packages = symbol.candidate_packages();

    if (candidate_packages.empty()) {
        assert(!symbol.package_file());
        if (output_status == OutputStatus::Undefined) {
            if (verbosity >= LogLevel::DEBUG) {
                std::cerr << "Entity `" << symbol.name() << "` from `" << input_file.path().u8string()
                          << "` is not used" << std::endl;
            }
            return;
        }

        assert(false);
        return;
    }

    const bool within_closure_depth = symbol.reference_level() <= Config::closure_depth();
    const bool referenced = output_status == OutputStatus::Referenced;
    const bool root_or_referenced = referenced || output_status == OutputStatus::Root;

    if (candidate_packages.size() == 1) {
        const auto& candidate_package = **candidate_packages.begin();

        if (verbosity >= LogLevel::DEBUG) {
            std::cerr << "Entity `" << symbol.name() << "` from `" << input_file.path().u8string();
            if (symbol.matches_any_filters()) {
                std::cerr << "` matches a filter for `";
            } else if (referenced) {
                std::cerr << "` is only used from `";
            } else {
                assert(false);
            }
            std::cerr << candidate_package.cangjie_name() << "` package, ";
            if (const auto* package_file = symbol.package_file()) {
                assert(symbol.package_file());
                std::cerr << "assigned `" << package_file->output_path().u8string() << '`';
            } else if (!within_closure_depth && root_or_referenced) {
                assert(!symbol.package_file());
                std::cerr << "but has " << SymbolDepthPrinter(symbol)
                          << " and was filtered out by --closure-depth=" << Config::closure_depth();
            } else if (within_closure_depth && !root_or_referenced) {
                assert(!symbol.package_file());
                std::cerr << "but was not referenced";
            } else {
                assert(!symbol.package_file());
                std::cerr << "but was not referenced, has " << SymbolDepthPrinter(symbol)
                          << " and was filtered out by --closure-depth=" << Config::closure_depth();
            }
            std::cerr << std::endl;
        }

        return;
    }

    assert(!symbol.package_file());

    std::cerr << "Entity `" << symbol.name() << "` from `" << input_file.path().u8string() << "` is ambiguous between "
              << candidate_packages.size() << " packages";

    static size_t printed_with_details = 0;

    if (verbosity > LogLevel::WARNING || ++printed_with_details <= 3) {
        std::cerr << ":\n";
        std::vector<std::string> reasons;
        for (const auto* package : candidate_packages) {
            std::cerr << "* " << package->cangjie_name();
            collect_ambiguity_reasons(*package, symbol, reasons);
            if (reasons.empty()) {
                std::cerr << " (?)" << std::endl;
            } else if (reasons.size() == 1) {
                std::cerr << " (" << reasons[0] << ')' << std::endl;
            } else if (reasons.size() == 2) {
                std::cerr << " (" << reasons[0] << ", " << reasons[1] << ')' << std::endl;
            } else {
                std::cerr << ':' << std::endl;
                for (auto&& reason : reasons) {
                    std::cerr << "    * " << reason << std::endl;
                }
            }
            reasons.clear();
        }

        if (within_closure_depth && !root_or_referenced) {
            std::cerr << "But it was not referenced" << std::endl;
        } else if (!within_closure_depth && root_or_referenced) {
            std::cerr << "But it has " << SymbolDepthPrinter(symbol)
                      << " and was filtered out by --closure-depth=" << Config::closure_depth() << std::endl;
        } else if (!within_closure_depth && !root_or_referenced) {
            std::cerr << "But it was not referenced, has " << SymbolDepthPrinter(symbol)
                      << " and was filtered out by --closure-depth=" << Config::closure_depth() << std::endl;
        }
    } else {
        if (within_closure_depth && !root_or_referenced) {
            std::cerr << ", but it was not referenced";
        } else if (!within_closure_depth && root_or_referenced) {
            std::cerr << ", but it has " << SymbolDepthPrinter(symbol)
                      << " and was filtered out by --closure-depth=" << Config::closure_depth();
        } else if (!within_closure_depth && !root_or_referenced) {
            std::cerr << ", but it was not referenced, has " << SymbolDepthPrinter(symbol)
                      << " and was filtered out by --closure-depth=" << Config::closure_depth();
        }
        std::cerr << ". Specify -v for more detailed information." << std::endl;
    }
}

static void mark_roots()
{
    auto& universe = Universe::get();
    for (auto& member : universe.top_level()) {
        set_package(member);
    }

    for (auto&& type : universe.types()) {
        // Omit types having no definition in source files (those are built-ins like
        // `id`).
        if (!type.defining_file()) {
            continue;
        }

        set_package(type);
    }
}

static void add_all_symbol_references()
{
    std::unordered_set<FileLevelSymbol*> candidates;
    for (const auto& input_file : inputs) {
        for (auto& symbol : input_file) {
            assert(symbol.defining_file());
            symbol.collect_referenced_symbols(candidates);
            for (auto* reference : candidates) {
                if (reference != &symbol && reference->defining_file()) {
                    if (symbol.add_reference(*reference) && verbosity >= LogLevel::TRACE) {
                        std::cerr << "Entity `" << symbol.name() << "` references `" << reference->name() << "`\n";
                    }
                }
            }
            candidates.clear();
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

static void symbol_references_to_packages_pass(FileLevelSymbol& symbol, bool& changed)
{
    assert(symbol.defining_file());

    if (symbol.output_status() == OutputStatus::Undefined) {
        return;
    }

    for (auto* reference : symbol.references_symbols()) {
        assert(reference);

        if (reference->add_referencing_symbol(symbol)) {
            changed = true;
        }

        if (reference->matches_any_filters()) {
            continue;
        }

        for (auto* package : symbol.candidate_packages()) {
            if (reference->add_candidate_package(*package)) {
                changed = true;
            }
        }
    }
}

static void symbol_references_to_packages()
{
    size_t iterations = 0;
    bool changed;
    do {
        changed = false;
        ++iterations;
        for (const auto& input_file : inputs) {
            for (auto& symbol : input_file) {
                symbol_references_to_packages_pass(symbol, changed);
            }
        }
    } while (changed);

    if (verbosity >= LogLevel::DIAGNOSTIC) {
        std::cerr << "Symbols were assigned to Cangjie packages in " << iterations << " iterations" << std::endl;
    }
}

[[nodiscard]] static bool register_symbols_in_declaration_order()
{
    auto success = true;

    for (const auto& input_file : inputs) {
        for (auto& symbol : input_file) {
            register_symbol_for_package(symbol, success);
        }
    }

    for (const auto& input_file : inputs) {
        for (auto& symbol : input_file) {
            diagnose_symbol(symbol);
        }
    }

    if (!success) {
        return false;
    }

    for (const auto& input_file : inputs) {
        for (auto& symbol : input_file) {
            if (auto* package_file = symbol.package_file()) {
                package_file->add_symbol(symbol);
            }
        }
    }

    return true;
}

bool mark_package()
{
    mark_roots();
    add_all_symbol_references();
    set_root_reference_level();
    symbol_references_to_packages();
    return register_symbols_in_declaration_order();
}

} // namespace objcgen
