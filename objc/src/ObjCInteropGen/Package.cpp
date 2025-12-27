// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "PackageConfig.h"

#include <iostream>

#include "Config.h"
#include "FatalException.h"

namespace objcgen {

Packages packages;

const std::string& PackageFilter::package_name() const noexcept
{
    return package_.cangjie_name();
}

PackageFile::PackageFile(std::string file_name, Package& package)
    : file_name_(std::move(file_name)),
      output_path_(package.output_path() + '/' + file_name_ + ".cj"),
      package_(&package)
{
    assert(!file_name_.empty());
    package.add_file(*this);
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
    if (!filters_it->second.is<toml::Table>()) {
        fatal("`packages` entry ", name_desc, " property `filters` should be a TOML table");
    }
    if (packages.by_cangjie_name(package_cangjie_name)) {
        fatal("There are multiple `packages` entries with the same `package-name` value `", package_cangjie_name, '`');
    }

    auto& package = *new Package(package_cangjie_name, compute_output_path(name_desc, config, package_cangjie_name));
    package.set_filters(create_filter(package, filters_it->second.as<toml::Table>()));

    packages.insert(package);
}

void create_packages()
{
    if (const auto* packages = config.find("packages")) {
        std::size_t i = 0;
        for (auto&& package_any : packages->as<toml::Array>()) {
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
}

} // namespace objcgen
