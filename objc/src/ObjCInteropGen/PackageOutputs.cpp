// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "PackageConfig.h"

#include <iostream>

#include "Config.h"
#include "FatalException.h"
#include "Logging.h"
#include "Strings.h"

namespace objcgen {

static std::string compute_output_root(const std::string_view package_name)
{
    if (const auto* output_roots_any = config.find("output-roots")) {
        if (!output_roots_any->is<toml::Table>()) {
            fatal("`output-roots` should be a TOML table");
        }
        const auto& output_roots_table = output_roots_any->as<toml::Table>();
        if (output_roots_table.empty()) {
            fatal("`packages` entry ", package_name, " has no `output-path` specified and there are no `output-roots`");
        }

        if (output_roots_table.size() != 1) {
            fatal("`packages` entry ", package_name,
                " has no `output-path` or `output-root` specified and there are multiple `output-roots`");
        }

        const auto it = output_roots_table.cbegin();
        assert(it != output_roots_table.cend());
        return it->first;
    }

    fatal("`packages` entry ", package_name, " has no `output-path` specified and there are no `output-roots`");
}

static std::string compute_output_path_by_root_path(const std::string_view package_name,
    const std::string_view output_root_name, const std::string_view output_path, std::string_view package_cangjie_name)
{
    if (verbosity >= LogLevel::DIAGNOSTIC) {
        std::cerr << "`packages` entry " << package_name << " uses `output-roots` entry `" << output_root_name
                  << "` with `path` set to `" << output_path << "`, concatenating with `package-name` set to `"
                  << package_cangjie_name << "`" << std::endl;
    }

    auto result = std::string(output_path);
    if (!ends_with(result, "/") && !ends_with(result, "\\")) {
        result.append("/");
    }

    while (ends_with(package_cangjie_name, ".")) {
        package_cangjie_name = std::string_view(package_cangjie_name.data(), package_cangjie_name.size() - 1);
    }

    std::size_t pos = 0;
    while (pos != std::string::npos) {
        const auto dot = package_cangjie_name.find('.', pos);
        if (dot == std::string::npos) {
            result.append(package_cangjie_name, pos, dot);
            pos = std::string::npos;
        } else {
            result.append(package_cangjie_name, pos, dot - pos);
            result.append("/");
            pos = dot + 1;
        }
    }

    return result;
}

static std::string compute_output_path_by_root_name(const std::string_view package_name,
    const std::string_view output_root_name, const std::string_view package_cangjie_name)
{
    if (const auto* output_roots_any = config.find("output-roots")) {
        if (!output_roots_any->is<toml::Table>()) {
            fatal("`output-roots` should be a TOML table");
        }
        for (auto&& [output_root_key, output_root_any] : output_roots_any->as<toml::Table>()) {
            if (output_root_key != output_root_name) {
                continue;
            }

            if (!output_root_any.is<toml::Table>()) {
                fatal("`output-roots` entry `", output_root_name, "` should be a TOML table");
            }
            const auto output_path = get_string_value(output_root_any.as<toml::Table>(), package_name, "path",
                [&output_root_name](const auto&) -> std::string {
                    fatal("`output-roots` entry `", output_root_name, "` should define a `path` property");
                });

            return compute_output_path_by_root_path(package_name, output_root_name, output_path, package_cangjie_name);
        }

        fatal("`packages` entry ", package_name, " has no `output-path` specified and `output-root` `",
            output_root_name, "` was not found");
    }

    fatal("`packages` entry ", package_name, " has no `output-path` specified and there are no `output-roots`");
}

std::string compute_output_path(
    const std::string& name, const toml::Table& config, const std::string_view package_cangjie_name)
{
    return get_string_value(config, name, "output-path", [&name, &package_cangjie_name](const auto& config) {
        const auto output_root =
            get_string_value(config, name, "output-root", [&name](const auto&) { return compute_output_root(name); });

        return compute_output_path_by_root_name(name, output_root, package_cangjie_name);
    });
}

} // namespace objcgen
