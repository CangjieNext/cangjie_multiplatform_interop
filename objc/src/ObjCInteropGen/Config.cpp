// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "Config.h"

#include <filesystem>
#include <unordered_map>

#include "FatalException.h"
#include "Logging.h"
#include "TomlParseError.h"

namespace objcgen {

toml::Value config;

static void append_to_left(toml::Value& lhs, const toml::Array& rhs)
{
    assert(lhs.is<toml::Array>());
    for (auto&& item : rhs) {
        lhs.push(item);
    }
}

static void merge_to_left_array(toml::Value& lhs, const toml::Value& rhs, const std::string& property_name)
{
    assert(lhs.is<toml::Table>());
    assert(rhs.is<toml::Table>());
    auto* lhs_array = lhs.find(property_name);
    if (lhs_array && !lhs_array->is<toml::Array>()) {
        fatal("TOML property `", property_name, "` should be an array");
    }

    const auto* rhs_array = rhs.find(property_name);
    if (rhs_array && !rhs_array->is<toml::Array>()) {
        fatal("TOML property `", property_name, "` should be an array");
    }

    if (lhs_array) {
        if (rhs_array) {
            append_to_left(*lhs_array, rhs_array->as<toml::Array>());
        }
        return;
    }

    if (rhs_array) {
        lhs.set(property_name, *rhs_array);
    }
}

static void merge_to_left(toml::Value& lhs, const toml::Table& rhs)
{
    // No need to merge:
    // * output-roots
    // * sources, sources-mixins
    // Imports are "merged" by the caller
    merge_to_left_array(lhs, rhs, "packages");
    merge_to_left_array(lhs, rhs, "mappings");
}

class TomlFileParser {
public:
    [[nodiscard]] toml::Value parse(const std::string& path);

private:
    std::vector<std::string> import_sequence;
    std::unordered_map<std::string, std::vector<std::string>> imported;
};

[[nodiscard]] static std::string print_import_sequence(const std::vector<std::string>& import_sequence)
{
    assert(!import_sequence.empty());
    auto it = import_sequence.begin();
    auto end = import_sequence.end();
    auto message = '`' + *it + '`';
    for (++it; it != end; ++it) {
        message += " -> `";
        message += *it;
        message += '`';
    }
    return message;
}

toml::Value TomlFileParser::parse(const std::string& path)
{
    auto absolute_path = std::filesystem::absolute(path).u8string();
    if (verbosity >= LogLevel::INFO) {
        std::cerr << "Reading TOML file `" << absolute_path << '`' << std::endl;
    }

    for (const auto& p : import_sequence) {
        if (std::filesystem::absolute(p) == absolute_path) {
            import_sequence.push_back(path);
            fatal('`', absolute_path, "`: recursive import: ", print_import_sequence(import_sequence));
        }
    }
    import_sequence.push_back(path);
    auto [it, new_path] = imported.try_emplace(absolute_path, import_sequence);
    if (!new_path) {
        std::cerr << '`' << absolute_path
                  << "`: multiple import\n"
                     "  First import: "
                  << print_import_sequence(it->second)
                  << "\n"
                     "  Additional import (ignored): "
                  << print_import_sequence(import_sequence) << std::endl;
        import_sequence.resize(import_sequence.size() - 1);
        return {};
    }

    if (!std::filesystem::exists(path)) {
        fatal("TOML file `", path, "` doesn't exist");
    }

    auto parse_result = toml::parseFile(path);
    if (!parse_result.valid()) {
        throw TomlParseError(absolute_path, parse_result.errorReason);
    }
    assert(parse_result.value.is<toml::Table>());
    if (const auto* imports_any = parse_result.value.find("imports")) {
        if (!imports_any->is<toml::Array>()) {
            fatal("`imports` in `", path, "` should be a TOML array of strings");
        }
        std::size_t i = 0;
        for (auto&& item_any : imports_any->as<toml::Array>()) {
            if (!item_any.is<std::string>()) {
                fatal("`imports` in `", path, "` item #", i, " should be a string");
            }
            const auto& import_path = item_any.as<std::string>();
            if (import_path.empty()) {
                fatal("`imports` in `", path, "` item #", i, " is empty");
            }

            auto import_config = parse(import_path);
            if (!import_config.empty()) {
                assert(import_config.is<toml::Table>());

                if (verbosity >= LogLevel::INFO) {
                    std::cerr << "Merging TOML file `" << import_path << "` into `" << path << '`' << std::endl;
                }

                merge_to_left(parse_result.value, import_config.as<toml::Table>());
            }
            i++;
        }
    }
    import_sequence.resize(import_sequence.size() - 1);

    return parse_result.value;
}

void parse_toml_config_file(const std::string& path)
{
    TomlFileParser parser;
    config = parser.parse(path);
}

} // namespace objcgen
