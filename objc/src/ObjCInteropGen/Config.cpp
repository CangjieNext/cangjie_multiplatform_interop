// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "Config.h"

#include <filesystem>
#include <unordered_map>

#include "ExpandString.h"
#include "FatalException.h"
#include "Logging.h"
#include "TomlParseError.h"

namespace objcgen {

static toml::Value g_config;
static ClosureDepthType g_closure_depth;

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

struct ImportEntry final {
    std::string original_path;
    std::filesystem::path absolute_path;

    ImportEntry(std::string original_path, const std::filesystem::path& modified_path)
        : original_path(std::move(original_path))
    {
        absolute_path = std::filesystem::absolute(modified_path);
    }

    [[nodiscard]] std::string to_string() const
    {
        if (verbosity >= LogLevel::DIAGNOSTIC) {
            return '`' + original_path + "` (`" + absolute_path.u8string() + "`)";
        }
        return '`' + original_path + '`';
    }
};

class TomlFileParser {
public:
    [[nodiscard]] toml::Value parse(const ImportEntry& path);

private:
    std::vector<ImportEntry> import_sequence;
    std::unordered_map<std::string, std::vector<ImportEntry>> imported;
};

[[nodiscard]] static std::string print_import_sequence(const std::vector<ImportEntry>& import_sequence)
{
    assert(!import_sequence.empty());
    auto it = import_sequence.begin();
    auto end = import_sequence.end();
    auto message = it->to_string();
    for (++it; it != end; ++it) {
        message += " -> ";
        message += it->to_string();
    }
    return message;
}

toml::Value TomlFileParser::parse(const ImportEntry& path)
{
    if (verbosity >= LogLevel::INFO) {
        std::cerr << "Reading TOML file " << path.to_string() << std::endl;
    }

    if (!std::filesystem::exists(path.absolute_path)) {
        fatal("TOML file ", path.to_string(), " doesn't exist");
    }

    if (std::filesystem::is_directory(path.absolute_path)) {
        fatal("TOML path ", path.to_string(), " is a directory");
    }

    for (const auto& p : import_sequence) {
        if (p.absolute_path == path.absolute_path) {
            import_sequence.push_back(path);
            fatal(path.to_string(), ": recursive import: ", print_import_sequence(import_sequence));
        }
    }

    import_sequence.push_back(path);

    auto absolute_path = path.absolute_path.u8string();
    auto [it, new_path] = imported.try_emplace(absolute_path, import_sequence);
    if (!new_path) {
        std::cerr << path.to_string() << ": multiple import\n"
                  << "  First import: " << print_import_sequence(it->second) << "\n"
                  << "  Additional import (ignored): " << print_import_sequence(import_sequence) << std::endl;
        import_sequence.erase(import_sequence.end() - 1);
        return {};
    }

    auto parse_result = toml::parseFile(absolute_path);
    if (!parse_result.valid()) {
        throw TomlParseError(absolute_path, parse_result.errorReason);
    }
    assert(parse_result.value.is<toml::Table>());
    if (const auto* imports_any = parse_result.value.find("imports")) {
        if (!imports_any->is<toml::Array>()) {
            fatal("`imports` in ", path.to_string(), " should be a TOML array of strings");
        }

        assert(path.absolute_path.has_parent_path());
        std::filesystem::path import_path_root = path.absolute_path.parent_path();

        std::size_t i = 0;
        for (auto&& item_any : imports_any->as<toml::Array>()) {
            if (!item_any.is<std::string>()) {
                fatal("`imports` in ", path.to_string(), " item #", i, " should be a string");
            }
            const auto& import_path_original_string = item_any.as<std::string>();
            if (import_path_original_string.empty()) {
                fatal("`imports` in ", path.to_string(), " item #", i, " is empty");
            }

            const std::filesystem::path import_path_original = expand_string(import_path_original_string);
            std::filesystem::path import_path_modified;
            if (import_path_original.is_absolute()) {
                import_path_modified = import_path_original;
            } else {
                import_path_modified = import_path_root / import_path_original;

                if (!std::filesystem::exists(import_path_modified) && std::filesystem::exists(import_path_original)) {
                    std::cerr << "Consider using $PWD to reference TOML file `" << import_path_original_string
                              << "` from " << path.to_string() << std::endl;
                }
            }

            ImportEntry import_path{import_path_original_string, import_path_modified};
            auto import_config = parse(import_path);
            if (!import_config.empty()) {
                assert(import_config.is<toml::Table>());

                if (verbosity >= LogLevel::INFO) {
                    std::cerr << "Merging TOML file " << import_path.to_string() << " into " << path.to_string()
                              << std::endl;
                }

                merge_to_left(parse_result.value, import_config.as<toml::Table>());
            }
            i++;
        }
    }

    import_sequence.erase(import_sequence.end() - 1);
    return parse_result.value;
}

void Config::parse_from_toml_file(const std::string& path)
{
    g_config = TomlFileParser().parse(ImportEntry{path, path});
    assert(g_config.is<toml::Table>());
    const auto* closure_depth_value = g_config.find("closure-depth");
    if (closure_depth_value) {
        auto int_closure_depth = closure_depth_value->as<int64_t>();
        g_closure_depth =
            int_closure_depth < 0 ? UNLIMITED_CLOSURE_DEPTH : static_cast<ClosureDepthType>(int_closure_depth);
    } else {
        g_closure_depth = UNLIMITED_CLOSURE_DEPTH;
    }
}

const toml::Value* Config::find(const std::string& key)
{
    assert(g_config.is<toml::Table>());
    return g_config.find(key);
}

ClosureDepthType Config::closure_depth() noexcept
{
    assert(g_config.is<toml::Table>());
    return g_closure_depth;
}

} // namespace objcgen
