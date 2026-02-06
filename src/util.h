/**
    @file
    @author Alexander Sherikov
    @copyright 2025-2026 Alexander Sherikov. Licensed under the Apache License,
    Version 2.0. (see LICENSE or http://www.apache.org/licenses/LICENSE-2.0)
    @brief
*/

#pragma once

namespace pjmsg_mcap_wrapper
{
    template <typename... t_String>
    std::string str_concat(t_String &&...strings)
    {
        std::string result;
        (result += ... += std::forward<t_String>(strings));
        return result;
    }
}  // namespace pjmsg_mcap_wrapper

#define SHARF_THROW_IF(condition, ...)                                                                                 \
    if (condition)                                                                                                     \
    {                                                                                                                  \
        throw std::runtime_error(str_concat(__VA_ARGS__));                                                             \
    }
