// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "PackageConfig.h"

#include <iostream>

#include "Config.h"
#include "ExpandString.h"
#include "FatalException.h"

namespace objcgen {

static Packages packages;
static Roots roots;

const std::string& PackageFilter::package_name() const noexcept
{
    return package_.cangjie_name();
}

bool RootFilter::set_package(Package* package)
{
    if (package_ != nullptr) {
        return false;
    }
    package_ = package;
    return true;
}

[[nodiscard]] const PackageFilter& RootFilter::as_filter() const noexcept
{
    assert(package_);
    return create_filter(*package_, toml::Value(pattern_));
}

PackageFile::PackageFile(std::string file_name, Package& package)
    : output_path_(std::filesystem::u8path(package.output_path() + '/' + file_name + ".cj")), package_(&package)
{
    assert(!file_name.empty());
}

bool Package::is_root(const FileLevelSymbol& symbol) const noexcept
{
    const auto& name = symbol.objc_name();

    // When `root-filters` is present, only those matches become closure roots.
    if (const auto* filter = root_filters()) {
        return filter->apply(name);
    }
    // If there are global roots, other filter properties cannot act as roots.
    if (!roots.empty()) {
        return false;
    }

    // Otherwise, other filter properties act as roots.
    if (const auto* filter = filters(); filter && filter->apply(name)) {
        return true;
    }

    return false;
}

const Packages& Packages::get() noexcept
{
    return packages;
}

static void create_package(std::size_t package_index, const toml::Table& config)
{
    std::string name_desc = '#' + std::to_string(package_index);

    auto package_cangjie_name_opt = get_string_value(config, name_desc, "package-name");
    if (!package_cangjie_name_opt) {
        fatal("`packages` entry ", name_desc, " should define `package-name` property");
    }
    const auto& package_cangjie_name = *package_cangjie_name_opt;

    name_desc = '`' + package_cangjie_name + '`';

    auto filters_it = config.find("filters");
    if (filters_it == config.end()) {
        fatal("`packages` entry ", name_desc, " should define `filters` property");
    }
    if (packages.by_cangjie_name(package_cangjie_name)) {
        fatal("There are multiple `packages` entries with the same `package-name` value `", package_cangjie_name, '`');
    }

    const auto output_path = compute_output_path(name_desc, config, package_cangjie_name);
    auto& package = *new Package(package_cangjie_name, expand_string(output_path));
    const auto& filter = create_filter(package, filters_it->second);
    bool success = true;
    for (auto& root : roots) {
        if (filter.apply(root.pattern())) {
            if (!root.set_package(&package)) {
                success = false;
                std::cerr << "The " << root.pattern() << " root is assigned to both " << root.package()->cangjie_name()
                          << " and " << package_cangjie_name << " packages" << std::endl;
            }
        }
    }
    if (!success) {
        fatal("Every root should be assigned to only one package. See the detailed error message above.");
    }
    package.set_filters(filter);
    packages.insert(package);
}

void create_packages()
{
    // Create roots
    if (const auto* roots_value = Config::find("roots")) {
        for (auto&& root_pattern : roots_value->as<toml::Array>()) {
            if (!root_pattern.is<std::string>()) {
                fatal("`roots` entry is not a TOML string");
            }
            roots.register_from_value(root_pattern.as<std::string>());
        }
    }

    // Create packages
    if (const auto* package_entries = Config::find("packages")) {
        std::size_t i = 0;
        for (auto&& package_any : package_entries->as<toml::Array>()) {
            if (!package_any.is<toml::Table>()) {
                fatal("`packages` entry #", i, " is not a TOML table");
            }
            // It makes sense to consider supporting packages-mixins
            create_package(i, package_any.as<toml::Table>());
            i++;
        }
    } else {
        fatal("`packages` should be a TOML array of tables");
    }

    // Check roots
    bool success = true;
    for (auto& root : roots) {
        const auto root_package = root.package();
        if (root_package == nullptr) {
            success = false;
            std::cerr << "No package found for the root " << root.pattern() << std::endl;
            continue;
        }
        if (!success) {
            continue;
        }
        if (const auto package_root_filters = root.package()->root_filters(); package_root_filters != nullptr) {
            root_package->set_root_filters(merge_filters(*package_root_filters, root.as_filter()));
        } else {
            root_package->set_root_filters(root.as_filter());
        }
    }
    if (!success) {
        fatal("Every root should have a package. See the detailed error message above.");
    }
}

} // namespace objcgen
