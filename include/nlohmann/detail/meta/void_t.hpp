//     __ _____ _____ _____
//  __|  |   __|     |   | |  JSON for Modern C++
// |  |  |__   |  |  | | | |  version 3.11.3
// |_____|_____|_____|_|___|  https://github.com/nlohmann/json
//
// SPDX-FileCopyrightText: 2013-2023 Niels Lohmann <https://nlohmann.me>
// SPDX-License-Identifier: MIT

#pragma once

#include <nlohmann/detail/abi_macros.hpp>

NLOHMANN_JSON_NAMESPACE_BEGIN
namespace detail
{

template<typename ...Ts> struct make_void
{
    using type = void;
};
template<typename ...Ts> using void_t = typename make_void<Ts...>::type;

}  // namespace detail
NLOHMANN_JSON_NAMESPACE_END

// void_t提供一种对模板参数的包装，配合模板参数自身的推到，利用 SFINAE 选择合适的特化版本。
// void_t 的目的是在模板中引入一个总是为 void 类型的成员 type，无论传递给 void_t 的模板参数是什么。
// 这样，void_t 的作用是创建一个总是为 void 类型的别名模板，用于在模板中触发 SFINAE 机制。
