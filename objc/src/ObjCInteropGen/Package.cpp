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

std::string_view PackageFilter::package_name() const
{
    assert(package_);
    return package_->cangjie_name();
}

PackageFile::PackageFile(std::string file_name, Package* package) : file_name_(std::move(file_name)), package_(package)
{
    assert(!file_name_.empty());
    assert(package);

    auto path = std::string(package->output_path());
    path += "/";
    path += file_name_;
    path += ".cj";
    output_path_ = path;
    package->add_file(this);
}

PackageFilesIterator::reference PackageFilesIterator::get() const
{
    return *it_->second;
}

static Package* create_package(std::size_t package_index, const toml::table& config)
{
    std::string name_desc = "#" + std::to_string(package_index);

    auto package_cangjie_name =
        get_string_value(config, name_desc, "package-name", [&name_desc](const toml::table&) -> std::string {
            fatal("`packages` entry ", name_desc, " should define `package-name` property");
        });

    name_desc = "`" + package_cangjie_name + "`";

    auto output_path = compute_output_path(name_desc, config, package_cangjie_name);

    const toml::table* filters;
    if (auto* filters_any = config.get("filters")) {
        if (auto* filters_table = filters_any->as_table()) {
            filters = filters_table;
        } else {
            fatal("`packages` entry ", name_desc, " property `filters` should be a TOML table");
        }
    } else {
        fatal("`packages` entry ", name_desc, " should define `filters` property");
    }

    assert(filters);

    if (packages.by_cangjie_name(package_cangjie_name)) {
        fatal("There are multiple `packages` entries with the same `package-name` value `", package_cangjie_name, "`");
    }

    auto* package = new Package(package_cangjie_name, output_path);
    package->set_filters(create_filter(package, *filters));

    packages.insert(package);
    return package;
}

PackagesIterator::reference PackagesIterator::get() const
{
    return *it_->second;
}

void create_packages()
{
    if (auto* packages = config.get_as<toml::array>("packages")) {
        std::size_t i = 0;
        for (auto&& package_any : *packages) {
            if (const auto* package = package_any.as_table()) {
                // TODO: consider supporting packages-mixins
                create_package(i, *package);
            } else {
                fatal("`packages` entry #", i, " is not a TOML table");
            }
            i++;
        }
    } else {
        fatal("`packages` should be a TOML array of tables");
    }
}

} // namespace objcgen
