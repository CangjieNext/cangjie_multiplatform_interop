// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "Logging.h"

namespace objcgen {

LogLevel verbosity = LogLevel::WARNING;

std::string_view describe_regex_error(std::regex_constants::error_type type)
{
    switch (type) {
        case std::regex_constants::error_collate:
            return "The expression contains an invalid collating element name.";
        case std::regex_constants::error_ctype:
            return "The expression contains an invalid character class name.";
        case std::regex_constants::error_escape:
            return "The expression contains an invalid escaped character, or a trailing escape.";
        case std::regex_constants::error_backref:
            return "The expression contains an invalid back reference.";
        case std::regex_constants::error_brack:
            return "The expression contains mismatched [ and ].";
        case std::regex_constants::error_paren:
            return "The expression contains mismatched ( and ).";
        case std::regex_constants::error_brace:
            return "The expression contains mismatched { and }.";
        case std::regex_constants::error_badbrace:
            return "The expression contains an invalid range in a {} expression.";
        case std::regex_constants::error_range:
            return "The expression contains an invalid character range, such as [b-a].";
        case std::regex_constants::error_space:
            return "There was insufficient memory to convert the expression into a finite state machine.";
        case std::regex_constants::error_badrepeat:
            return "One of *?+{ was not preceded by a valid regular expression.";
        case std::regex_constants::error_complexity:
            return "The complexity of an attempted match against a regular expression exceeded a predefined level.";
        case std::regex_constants::error_stack:
            return "There was insufficient memory to perform a regular expression match.";
        default:
            return "Unknown regular expression error.";
    }
}

} // namespace objcgen
