// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include <iostream>
#include <string_view>

#include "CangjieWriter.h"
#include "Config.h"
#include "Diagnostics.h"
#include "FatalException.h"
#include "Logging.h"
#include "Mappings.h"
#include "MarkPackage.h"
#include "Mode.h"
#include "Package.h"
#include "SourceScannerConfig.h"
#include "Strings.h"
#include "Transform.h"
#include "Universe.h"

// clang -fobjc-runtime=gnustep `gnustep-config --objc-flags` -Xclang -ast-dump -c M.m -o M.o -v > ast.txt

using namespace objcgen;

static void show_help(const char* executable)
{
    std::cout << "Usage: " << (executable ? std::filesystem::path(executable).filename().string() : "ObjCInteropGen")
              << " [-v] config-file.toml\n";
    std::cout << "    -v\n";
    std::cout << "        increase logging verbosity level (can be applied multiple times)\n";
}

static std::optional<std::string_view> get_arg_value(const char* const argv[], int& arg_index, std::string_view name)
{
    std::string_view arg = argv[arg_index];
    if (starts_with(arg, name)) {
        auto name_size = name.size();
        if (arg.size() == name_size) {
            auto next = argv[arg_index + 1];
            if (!next) {
                return std::string_view();
            }
            ++arg_index;
            return next;
        }
        if (arg[name_size] == '=') {
            return arg.substr(name_size + 1);
        }
    }
    return std::nullopt;
}

int main(int argc, char* argv[])
{
    std::string_view stage = "Parsing command line options";

    try {
        if (argc <= 1) {
            show_help(argv[0]);
            return 1;
        }
        if (argc == 2) {
            std::string_view arg = argv[1];
            if (arg == "--help" || arg == "-?" || arg == "-h") {
                show_help(argv[0]);
                return 0;
            }
        }
        std::size_t verbosityVal = 0;
        bool config_specified = false;
        for (int i = 1; i < argc; i++) {
            std::string_view arg = argv[i];
            if (starts_with(arg, "-v")) {
                verbosityVal += arg.length() - 1;
                verbosity = static_cast<LogLevel>(verbosityVal);
                continue;
            }

            if (arg == "--generate-definitions") {
                mode = Mode::GENERATE_DEFINITIONS;
                continue;
            }

            auto mode_string = get_arg_value(argv, i, "--mode");
            if (mode_string) {
                if (mode_string == "normal") {
                    mode = Mode::NORMAL;
                } else if (mode_string == "experimental") {
                    mode = Mode::EXPERIMENTAL;
                } else if (mode_string == "generate-definitions") {
                    mode = Mode::GENERATE_DEFINITIONS;
                } else {
                    std::cerr << "Unknown mode \"" << *mode_string << "\"\n";
                    return 1;
                }
                continue;
            }

            if (ends_with(arg, ".toml")) {
                if (config_specified) {
                    std::cerr << "Multiple .toml files specified\n";
                    return 1;
                }
                config_specified = true;
                parse_toml_config_file(arg);
                continue;
            }

            show_help(argv[0]);
            return 1;
        }

        if (!config_specified) {
            show_help(argv[0]);
            return 1;
        }

        stage = "Parsing Objective-C sources";
        parse_sources();

        stage = "Creating Cangjie packages";
        create_packages();
        if (!mark_package()) {
            return 1;
        }

        stage = "Transforming sources";
        check_marked_symbols();

        initialize_mappings();

        apply_transforms();

        stage = "Writing Cangjie outputs";
        write_cangjie();
    } catch (const toml::parse_error& e) {
        std::cerr << e << std::endl;
        return 1;
    } catch (const FatalException&) {
        // FatalException is assumed to print the error message before throwing
        return 1;
    } catch (const std::exception& e) {
        std::cerr << stage << ":\n" << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << stage << ":\n"
                  << "Unknown exception" << std::endl;
        return 1;
    }

    return 0;
}
