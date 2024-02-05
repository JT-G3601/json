//     __ _____ _____ _____
//  __|  |   __|     |   | |  JSON for Modern C++
// |  |  |__   |  |  | | | |  version 3.11.3
// |_____|_____|_____|_|___|  https://github.com/nlohmann/json
//
// SPDX-FileCopyrightText: 2013-2023 Niels Lohmann <https://nlohmann.me>
// SPDX-License-Identifier: MIT

#pragma once

#include <algorithm> // all_of
#include <cctype> // isdigit
#include <cerrno> // errno, ERANGE
#include <cstdlib> // strtoull
#ifndef JSON_NO_IO
    #include <iosfwd> // ostream
#endif  // JSON_NO_IO
#include <limits> // max
#include <numeric> // accumulate
#include <string> // string
#include <utility> // move
#include <vector> // vector

#include <nlohmann/detail/exceptions.hpp>
#include <nlohmann/detail/macro_scope.hpp>
#include <nlohmann/detail/string_concat.hpp>
#include <nlohmann/detail/string_escape.hpp>
#include <nlohmann/detail/value_t.hpp>

NLOHMANN_JSON_NAMESPACE_BEGIN

/// @brief JSON Pointer defines a string syntax for identifying a specific value within a JSON document
/// @sa https://json.nlohmann.me/api/json_pointer/
template<typename RefStringType>
class json_pointer
{
    // allow basic_json to access private members
    NLOHMANN_BASIC_JSON_TPL_DECLARATION
    friend class basic_json;

    template<typename>
    friend class json_pointer;
    // template<typename> 没有指定具体的模板参数
    // 通常被称为模板模板参数，在这种情况下，模板本身
    // 是一个参数，他期望接受一个模板作为实际的参数

    // 将自己声明为自己的友元是合法的，这样的声明可以在特定情况下是有用的，
    // 例如，当一个类的不同实例之间需要访问彼此的私有成员时。通过将自己声明为友元，
    // 类的不同实例之间可以共享对彼此私有成员的访问权限。

    template<typename T>
    struct string_t_helper
    {
        using type = T;
    };

    NLOHMANN_BASIC_JSON_TPL_DECLARATION
    struct string_t_helper<NLOHMANN_BASIC_JSON_TPL>
    {
        using type = StringType;
    };

  public:
    // for backwards compatibility accept BasicJsonType
    using string_t = typename string_t_helper<RefStringType>::type;

    /// @brief create JSON pointer
    /// @sa https://json.nlohmann.me/api/json_pointer/json_pointer/
    explicit json_pointer(const string_t& s = "")
        : reference_tokens(split(s))
    {}

    /// @brief return a string representation of the JSON pointer
    /// @sa https://json.nlohmann.me/api/json_pointer/to_string/
    string_t to_string() const
    {
        return std::accumulate(reference_tokens.begin(), reference_tokens.end(),
                               string_t{},
                               [](const string_t& a, const string_t& b)
        {
            return detail::concat(a, '/', detail::escape(b));
        });
    }

    /// @brief return a string representation of the JSON pointer
    /// @sa https://json.nlohmann.me/api/json_pointer/operator_string/
    JSON_HEDLEY_DEPRECATED_FOR(3.11.0, to_string())
    operator string_t() const
    {
        return to_string();
    }

#ifndef JSON_NO_IO
    /// @brief write string representation of the JSON pointer to stream
    /// @sa https://json.nlohmann.me/api/basic_json/operator_ltlt/
    friend std::ostream& operator<<(std::ostream& o, const json_pointer& ptr)
    {
        o << ptr.to_string();
        return o;
    }
#endif

    /// @brief append another JSON pointer at the end of this JSON pointer
    /// @sa https://json.nlohmann.me/api/json_pointer/operator_slasheq/
    json_pointer& operator/=(const json_pointer& ptr)
    {
        reference_tokens.insert(reference_tokens.end(),
                                ptr.reference_tokens.begin(),
                                ptr.reference_tokens.end());
        return *this;
    }

    /// @brief append an unescaped reference token at the end of this JSON pointer
    /// @sa https://json.nlohmann.me/api/json_pointer/operator_slasheq/
    json_pointer& operator/=(string_t token)
    {
        push_back(std::move(token));
        return *this;
    }

    /// @brief append an array index at the end of this JSON pointer
    /// @sa https://json.nlohmann.me/api/json_pointer/operator_slasheq/
    json_pointer& operator/=(std::size_t array_idx)
    {
        return *this /= std::to_string(array_idx);
    }

    /// @brief create a new JSON pointer by appending the right JSON pointer at the end of the left JSON pointer
    /// @sa https://json.nlohmann.me/api/json_pointer/operator_slash/
    friend json_pointer operator/(const json_pointer& lhs,
                                  const json_pointer& rhs)
    {
        return json_pointer(lhs) /= rhs;
    }

    /// @brief create a new JSON pointer by appending the unescaped token at the end of the JSON pointer
    /// @sa https://json.nlohmann.me/api/json_pointer/operator_slash/
    friend json_pointer operator/(const json_pointer& lhs, string_t token) // NOLINT(performance-unnecessary-value-param)
    {
        return json_pointer(lhs) /= std::move(token);
    }

    /// @brief create a new JSON pointer by appending the array-index-token at the end of the JSON pointer
    /// @sa https://json.nlohmann.me/api/json_pointer/operator_slash/
    friend json_pointer operator/(const json_pointer& lhs, std::size_t array_idx)
    {
        return json_pointer(lhs) /= array_idx;
    }

    /// @brief returns the parent of this JSON pointer
    /// @sa https://json.nlohmann.me/api/json_pointer/parent_pointer/
    json_pointer parent_pointer() const
    {
        if (empty())
        {
            return *this;
        }

        json_pointer res = *this;
        res.pop_back();
        return res;
    }

    /// @brief remove last reference token
    /// @sa https://json.nlohmann.me/api/json_pointer/pop_back/
    void pop_back()
    {
        if (JSON_HEDLEY_UNLIKELY(empty()))
        {
            JSON_THROW(detail::out_of_range::create(405, "JSON pointer has no parent", nullptr));
        }

        reference_tokens.pop_back();
    }

    /// @brief return last reference token
    /// @sa https://json.nlohmann.me/api/json_pointer/back/
    const string_t& back() const
    {
        if (JSON_HEDLEY_UNLIKELY(empty()))
        {
            JSON_THROW(detail::out_of_range::create(405, "JSON pointer has no parent", nullptr));
        }

        return reference_tokens.back();
    }

    /// @brief append an unescaped token at the end of the reference pointer
    /// @sa https://json.nlohmann.me/api/json_pointer/push_back/
    void push_back(const string_t& token)
    {
        reference_tokens.push_back(token);
    }

    /// @brief append an unescaped token at the end of the reference pointer
    /// @sa https://json.nlohmann.me/api/json_pointer/push_back/
    void push_back(string_t&& token)
    {
        reference_tokens.push_back(std::move(token));
    }
    // 对 token 使用 std::move() 转移所有权

    /// @brief return whether pointer points to the root document
    /// @sa https://json.nlohmann.me/api/json_pointer/empty/
    bool empty() const noexcept
    {
        return reference_tokens.empty();
    }

  private:
    /*!
    @param[in] s  reference token to be converted into an array index

    @return integer representation of @a s

    @throw parse_error.106  if an array index begins with '0'
    @throw parse_error.109  if an array index begins not with a digit
    @throw out_of_range.404 if string @a s could not be converted to an integer
    @throw out_of_range.410 if an array index exceeds size_type
    */
    template<typename BasicJsonType>
    static typename BasicJsonType::size_type array_index(const string_t& s)
    // static 表示该函数是一个静态成员函数，可以通过类名直接调用，而不需要创建实例。
    // typename BasicJsonType::size_type: 使用 BasicJsonType 类模板的 size_type 类型。
    // 这部分是为了使函数能够适用于不同的 JSON 类型，因为 JSON 类型可能定义了不同的 size_type。
    {
        using size_type = typename BasicJsonType::size_type;

        // error condition (cf. RFC 6901, Sect. 4)
        if (JSON_HEDLEY_UNLIKELY(s.size() > 1 && s[0] == '0'))
        {
            JSON_THROW(detail::parse_error::create(106, 0, detail::concat("array index '", s, "' must not begin with '0'"), nullptr));
        }

        // error condition (cf. RFC 6901, Sect. 4)
        if (JSON_HEDLEY_UNLIKELY(s.size() > 1 && !(s[0] >= '1' && s[0] <= '9')))
        {
            JSON_THROW(detail::parse_error::create(109, 0, detail::concat("array index '", s, "' is not a number"), nullptr));
        }

        const char* p = s.c_str();
        char* p_end = nullptr;
        errno = 0; // strtoull doesn't reset errno
        const unsigned long long res = std::strtoull(p, &p_end, 10); // NOLINT(runtime/int)
        if (p == p_end // invalid input or empty string
                || errno == ERANGE // out of range
                || JSON_HEDLEY_UNLIKELY(static_cast<std::size_t>(p_end - p) != s.size())) // incomplete read
        {
            JSON_THROW(detail::out_of_range::create(404, detail::concat("unresolved reference token '", s, "'"), nullptr));
        }

        // only triggered on special platforms (like 32bit), see also
        // https://github.com/nlohmann/json/pull/2203
        if (res >= static_cast<unsigned long long>((std::numeric_limits<size_type>::max)()))  // NOLINT(runtime/int)
        {
            JSON_THROW(detail::out_of_range::create(410, detail::concat("array index ", s, " exceeds size_type"), nullptr));   // LCOV_EXCL_LINE
        }

        return static_cast<size_type>(res);
    }

  JSON_PRIVATE_UNLESS_TESTED:
    json_pointer top() const
    {
        if (JSON_HEDLEY_UNLIKELY(empty()))
        {
            JSON_THROW(detail::out_of_range::create(405, "JSON pointer has no parent", nullptr));
        }

        json_pointer result = *this;
        result.reference_tokens = {reference_tokens[0]};
        return result;
    }

  private:
    /*!
    @brief create and return a reference to the pointed to value

    @complexity Linear in the number of reference tokens.

    @throw parse_error.109 if array index is not a number
    @throw type_error.313 if value cannot be unflattened
    */
    template<typename BasicJsonType>
    BasicJsonType& get_and_create(BasicJsonType& j) const
    {
        auto* result = &j;

        // in case no reference tokens exist, return a reference to the JSON value
        // j which will be overwritten by a primitive value
        for (const auto& reference_token : reference_tokens)
        {
            switch (result->type())
            {
                case detail::value_t::null:
                {
                    if (reference_token == "0")
                    {
                        // start a new array if reference token is 0
                        result = &result->operator[](0);
                    }
                    else
                    {
                        // start a new object otherwise
                        result = &result->operator[](reference_token);
                    }
                    break;
                }

                case detail::value_t::object:
                {
                    // create an entry in the object
                    result = &result->operator[](reference_token);
                    break;
                }

                case detail::value_t::array:
                {
                    // create an entry in the array
                    result = &result->operator[](array_index<BasicJsonType>(reference_token));
                    break;
                }

                /*
                The following code is only reached if there exists a reference
                token _and_ the current value is primitive. In this case, we have
                an error situation, because primitive values may only occur as
                single value; that is, with an empty list of reference tokens.
                */
                case detail::value_t::string:
                case detail::value_t::boolean:
                case detail::value_t::number_integer:
                case detail::value_t::number_unsigned:
                case detail::value_t::number_float:
                case detail::value_t::binary:
                case detail::value_t::discarded: // 如果 JSON 值的类型是字符串、布尔值、整数、无符号整数、
                                                 // 浮点数、二进制数据或者是一个被废弃的值，那么这些类型只
                                                 // 应该出现在 JSON 文档的叶子节点上。我们检查的是非叶子节点
                default:
                    JSON_THROW(detail::type_error::create(313, "invalid value to unflatten", &j));
            }
        }

        return *result;
    }

    /*!
    @brief return a reference to the pointed to value

    @note This version does not throw if a value is not present, but tries to
          create nested values instead. For instance, calling this function
          with pointer `"/this/that"` on a null value is equivalent to calling
          `operator[]("this").operator[]("that")` on that value, effectively
          changing the null value to an object.

    @param[in] ptr  a JSON value

    @return reference to the JSON value pointed to by the JSON pointer

    @complexity Linear in the length of the JSON pointer.

    @throw parse_error.106   if an array index begins with '0'
    @throw parse_error.109   if an array index was not a number
    @throw out_of_range.404  if the JSON pointer can not be resolved
    */
    template<typename BasicJsonType>
    BasicJsonType& get_unchecked(BasicJsonType* ptr) const
    {
        for (const auto& reference_token : reference_tokens)
        {
            // convert null values to arrays or objects before continuing
            if (ptr->is_null())
            {
                // check if reference token is a number
                const bool nums =
                    std::all_of(reference_token.begin(), reference_token.end(),
                                [](const unsigned char x)
                {
                    return std::isdigit(x);
                });

                // change value to array for numbers or "-" or to object otherwise
                *ptr = (nums || reference_token == "-")
                       ? detail::value_t::array
                       : detail::value_t::object;
            }

            switch (ptr->type())
            {
                case detail::value_t::object:
                {
                    // use unchecked object access
                    ptr = &ptr->operator[](reference_token);
                    break;
                }

                case detail::value_t::array:
                {
                    if (reference_token == "-")
                    {
                        // explicitly treat "-" as index beyond the end
                        ptr = &ptr->operator[](ptr->m_data.m_value.array->size());
                    }
                    else
                    {
                        // convert array index to number; unchecked access
                        ptr = &ptr->operator[](array_index<BasicJsonType>(reference_token));
                    }
                    break;
                }

                case detail::value_t::null:
                case detail::value_t::string:
                case detail::value_t::boolean:
                case detail::value_t::number_integer:
                case detail::value_t::number_unsigned:
                case detail::value_t::number_float:
                case detail::value_t::binary:
                case detail::value_t::discarded:
                default:
                    JSON_THROW(detail::out_of_range::create(404, detail::concat("unresolved reference token '", reference_token, "'"), ptr));
            }
        }

        return *ptr;
    }

    /*!
    @throw parse_error.106   if an array index begins with '0'
    @throw parse_error.109   if an array index was not a number
    @throw out_of_range.402  if the array index '-' is used
    @throw out_of_range.404  if the JSON pointer can not be resolved
    */
    template<typename BasicJsonType>
    BasicJsonType& get_checked(BasicJsonType* ptr) const
    {
        for (const auto& reference_token : reference_tokens)
        {
            switch (ptr->type())
            {
                case detail::value_t::object:
                {
                    // note: at performs range check
                    ptr = &ptr->at(reference_token);
                    break;
                }

                case detail::value_t::array:
                {
                    if (JSON_HEDLEY_UNLIKELY(reference_token == "-"))
                    // 在 JSON Pointer 中，- 用作数组索引的特殊标记。当 JSON Pointer 的引用路径指向一个数组时，
                    // 可以使用 - 作为引用标记，表示数组的末尾。在这个上下文中，- 不是一个有效的数组索引值，而是表示
                    // 数组的最后一个元素之后的位置。当引用标记为 - 时，会抛出一个 out_of_range 异常，因为 - 不能
                    // 作为一个有效的数组索引值。这是为了防止使用 - 引用数组的最后一个元素，而应该使用实际的索引值
                    {
                        // "-" always fails the range check
                        JSON_THROW(detail::out_of_range::create(402, detail::concat(
                                "array index '-' (", std::to_string(ptr->m_data.m_value.array->size()),
                                ") is out of range"), ptr));
                    }

                    // note: at performs range check
                    ptr = &ptr->at(array_index<BasicJsonType>(reference_token));
                    break;
                }

                case detail::value_t::null:
                case detail::value_t::string:
                case detail::value_t::boolean:
                case detail::value_t::number_integer:
                case detail::value_t::number_unsigned:
                case detail::value_t::number_float:
                case detail::value_t::binary:
                case detail::value_t::discarded:
                default:
                    JSON_THROW(detail::out_of_range::create(404, detail::concat("unresolved reference token '", reference_token, "'"), ptr));
            }
        }

        return *ptr;
    }

    /*!
    @brief return a const reference to the pointed to value

    @param[in] ptr  a JSON value

    @return const reference to the JSON value pointed to by the JSON
    pointer

    @throw parse_error.106   if an array index begins with '0'
    @throw parse_error.109   if an array index was not a number
    @throw out_of_range.402  if the array index '-' is used
    @throw out_of_range.404  if the JSON pointer can not be resolved
    */
    template<typename BasicJsonType>
    const BasicJsonType& get_unchecked(const BasicJsonType* ptr) const
    {
        for (const auto& reference_token : reference_tokens)
        {
            switch (ptr->type())
            {
                case detail::value_t::object:
                {
                    // use unchecked object access
                    ptr = &ptr->operator[](reference_token);
                    break;
                }

                case detail::value_t::array:
                {
                    if (JSON_HEDLEY_UNLIKELY(reference_token == "-"))
                    {
                        // "-" cannot be used for const access
                        JSON_THROW(detail::out_of_range::create(402, detail::concat("array index '-' (", std::to_string(ptr->m_data.m_value.array->size()), ") is out of range"), ptr));
                    }

                    // use unchecked array access
                    ptr = &ptr->operator[](array_index<BasicJsonType>(reference_token));
                    break;
                }

                case detail::value_t::null:
                case detail::value_t::string:
                case detail::value_t::boolean:
                case detail::value_t::number_integer:
                case detail::value_t::number_unsigned:
                case detail::value_t::number_float:
                case detail::value_t::binary:
                case detail::value_t::discarded:
                default:
                    JSON_THROW(detail::out_of_range::create(404, detail::concat("unresolved reference token '", reference_token, "'"), ptr));
            }
        }

        return *ptr;
    }

    /*!
    @throw parse_error.106   if an array index begins with '0'
    @throw parse_error.109   if an array index was not a number
    @throw out_of_range.402  if the array index '-' is used
    @throw out_of_range.404  if the JSON pointer can not be resolved
    */
    template<typename BasicJsonType>
    const BasicJsonType& get_checked(const BasicJsonType* ptr) const
    {
        for (const auto& reference_token : reference_tokens)
        {
            switch (ptr->type())
            {
                case detail::value_t::object:
                {
                    // note: at performs range check
                    ptr = &ptr->at(reference_token);
                    break;
                }

                case detail::value_t::array:
                {
                    if (JSON_HEDLEY_UNLIKELY(reference_token == "-"))
                    {
                        // "-" always fails the range check
                        JSON_THROW(detail::out_of_range::create(402, detail::concat(
                                "array index '-' (", std::to_string(ptr->m_data.m_value.array->size()),
                                ") is out of range"), ptr));
                    }

                    // note: at performs range check
                    ptr = &ptr->at(array_index<BasicJsonType>(reference_token));
                    break;
                }

                case detail::value_t::null:
                case detail::value_t::string:
                case detail::value_t::boolean:
                case detail::value_t::number_integer:
                case detail::value_t::number_unsigned:
                case detail::value_t::number_float:
                case detail::value_t::binary:
                case detail::value_t::discarded:
                default:
                    JSON_THROW(detail::out_of_range::create(404, detail::concat("unresolved reference token '", reference_token, "'"), ptr));
            }
        }

        return *ptr;
    }

    /*!
    @throw parse_error.106   if an array index begins with '0'
    @throw parse_error.109   if an array index was not a number
    */
    template<typename BasicJsonType>
    bool contains(const BasicJsonType* ptr) const
    {
        for (const auto& reference_token : reference_tokens)
        {
            switch (ptr->type())
            {
                case detail::value_t::object:
                {
                    if (!ptr->contains(reference_token))
                    {
                        // we did not find the key in the object
                        return false;
                    }

                    ptr = &ptr->operator[](reference_token);
                    break;
                }

                case detail::value_t::array:
                {
                    if (JSON_HEDLEY_UNLIKELY(reference_token == "-"))
                    {
                        // "-" always fails the range check
                        return false;
                    }
                    if (JSON_HEDLEY_UNLIKELY(reference_token.size() == 1 && !("0" <= reference_token && reference_token <= "9")))
                    {
                        // invalid char
                        return false;
                    }
                    if (JSON_HEDLEY_UNLIKELY(reference_token.size() > 1))
                    {
                        if (JSON_HEDLEY_UNLIKELY(!('1' <= reference_token[0] && reference_token[0] <= '9')))
                        {
                            // first char should be between '1' and '9'
                            return false;
                        }
                        for (std::size_t i = 1; i < reference_token.size(); i++)
                        {
                            if (JSON_HEDLEY_UNLIKELY(!('0' <= reference_token[i] && reference_token[i] <= '9')))
                            {
                                // other char should be between '0' and '9'
                                return false;
                            }
                        }
                    }

                    const auto idx = array_index<BasicJsonType>(reference_token);
                    if (idx >= ptr->size())
                    {
                        // index out of range
                        return false;
                    }

                    ptr = &ptr->operator[](idx);
                    break;
                }

                case detail::value_t::null:
                case detail::value_t::string:
                case detail::value_t::boolean:
                case detail::value_t::number_integer:
                case detail::value_t::number_unsigned:
                case detail::value_t::number_float:
                case detail::value_t::binary:
                case detail::value_t::discarded:
                default:
                {
                    // we do not expect primitive values if there is still a
                    // reference token to process
                    return false;
                }
            }
        }

        // no reference token left means we found a primitive value
        return true;
    }

    /*!
    @brief split the string input to reference tokens

    @note This function is only called by the json_pointer constructor.
          All exceptions below are documented there.

    @throw parse_error.107  if the pointer is not empty or begins with '/'
    @throw parse_error.108  if character '~' is not followed by '0' or '1'
    */
    static std::vector<string_t> split(const string_t& reference_string)
    {
        std::vector<string_t> result;

        // special case: empty reference string -> no reference tokens
        if (reference_string.empty())
        {
            return result;
        }

        // check if nonempty reference string begins with slash
        if (JSON_HEDLEY_UNLIKELY(reference_string[0] != '/'))
        {
            JSON_THROW(detail::parse_error::create(107, 1, detail::concat("JSON pointer must be empty or begin with '/' - was: '", reference_string, "'"), nullptr));
        }

        // extract the reference tokens:
        // - slash: position of the last read slash (or end of string)
        // - start: position after the previous slash
        for (
            // search for the first slash after the first character
            std::size_t slash = reference_string.find_first_of('/', 1),
            // set the beginning of the first reference token
            start = 1;
            // we can stop if start == 0 (if slash == string_t::npos)
            start != 0;
            // set the beginning of the next reference token
            // (will eventually be 0 if slash == string_t::npos)
            start = (slash == string_t::npos) ? 0 : slash + 1,
            // find next slash
            slash = reference_string.find_first_of('/', start))
        {
            // use the text between the beginning of the reference token
            // (start) and the last slash (slash).
            auto reference_token = reference_string.substr(start, slash - start);

            // check reference tokens are properly escaped
            for (std::size_t pos = reference_token.find_first_of('~');
                    pos != string_t::npos;
                    pos = reference_token.find_first_of('~', pos + 1))
            {
                JSON_ASSERT(reference_token[pos] == '~');

                // ~ must be followed by 0 or 1
                if (JSON_HEDLEY_UNLIKELY(pos == reference_token.size() - 1 ||
                                         (reference_token[pos + 1] != '0' &&
                                          reference_token[pos + 1] != '1')))
                {
                    JSON_THROW(detail::parse_error::create(108, 0, "escape character '~' must be followed with '0' or '1'", nullptr));
                }
            }
            // 在 JSON 中，斜杠 / 主要用于分隔对象的键，表示层级关系。
            // 在 JSON 中，波浪号 ~ 用作转义字符。它与后面的数字组合表示特定的字符，用于在键名中包含一些特殊字符。

            // finally, store the reference token
            detail::unescape(reference_token);
            result.push_back(reference_token);
        }

        return result;
    }

  private:
    /*!
    @param[in] reference_string  the reference string to the current value
    @param[in] value             the value to consider
    @param[in,out] result        the result object to insert values to

    @note Empty objects or arrays are flattened to `null`.
    */
    template<typename BasicJsonType>
    static void flatten(const string_t& reference_string,
                        const BasicJsonType& value,
                        BasicJsonType& result)
    {
        switch (value.type())
        {
            case detail::value_t::array:
            {
                if (value.m_data.m_value.array->empty())
                {
                    // flatten empty array as null
                    // 若数组为空，将引用路径和空指针添加到结果对象
                    result[reference_string] = nullptr;
                }
                else
                {
                    // iterate array and use index as reference string
                    // 遍历数组并使用索引构建新的引用路径，递归调用 flatten
                    for (std::size_t i = 0; i < value.m_data.m_value.array->size(); ++i)
                    {
                        flatten(detail::concat(reference_string, '/', std::to_string(i)),
                                value.m_data.m_value.array->operator[](i), result);
                    }
                }
                break;
            }

            case detail::value_t::object:
            {
                if (value.m_data.m_value.object->empty())
                {
                    // flatten empty object as null
                    result[reference_string] = nullptr;
                }
                else
                {
                    // iterate object and use keys as reference string
                    for (const auto& element : *value.m_data.m_value.object)
                    {
                        flatten(detail::concat(reference_string, '/', detail::escape(element.first)), element.second, result);
                    }
                }
                break;
            }

            case detail::value_t::null:
            case detail::value_t::string:
            case detail::value_t::boolean:
            case detail::value_t::number_integer:
            case detail::value_t::number_unsigned:
            case detail::value_t::number_float:
            case detail::value_t::binary:
            case detail::value_t::discarded:
            default:
            {
                // add primitive value with its reference string
                result[reference_string] = value;
                break;
            }
        }
    }

    /*!
    @param[in] value  flattened JSON

    @return unflattened JSON

    @throw parse_error.109 if array index is not a number
    @throw type_error.314  if value is not an object
    @throw type_error.315  if object values are not primitive
    @throw type_error.313  if value cannot be unflattened
    */
    template<typename BasicJsonType>
    static BasicJsonType
    unflatten(const BasicJsonType& value)
    {
        if (JSON_HEDLEY_UNLIKELY(!value.is_object()))
        {
            JSON_THROW(detail::type_error::create(314, "only objects can be unflattened", &value));
        }

        BasicJsonType result;

        // iterate the JSON object values
        for (const auto& element : *value.m_data.m_value.object)
        {
            if (JSON_HEDLEY_UNLIKELY(!element.second.is_primitive()))
            {
                JSON_THROW(detail::type_error::create(315, "values in object must be primitive", &element.second));
            }

            // assign value to reference pointed to by JSON pointer; Note that if
            // the JSON pointer is "" (i.e., points to the whole value), function
            // get_and_create returns a reference to result itself. An assignment
            // will then create a primitive value.
            json_pointer(element.first).get_and_create(result) = element.second;
        }

        return result;
    }

    // can't use conversion operator because of ambiguity
    json_pointer<string_t> convert() const&
    {
        json_pointer<string_t> result;
        result.reference_tokens = reference_tokens;
        return result;
    }

    json_pointer<string_t> convert()&&
    {
        json_pointer<string_t> result;
        result.reference_tokens = std::move(reference_tokens);
        return result;
    }

  public:
#if JSON_HAS_THREE_WAY_COMPARISON
    /// @brief compares two JSON pointers for equality
    /// @sa https://json.nlohmann.me/api/json_pointer/operator_eq/
    template<typename RefStringTypeRhs>
    bool operator==(const json_pointer<RefStringTypeRhs>& rhs) const noexcept
    {
        return reference_tokens == rhs.reference_tokens;
    }

    /// @brief compares JSON pointer and string for equality
    /// @sa https://json.nlohmann.me/api/json_pointer/operator_eq/
    JSON_HEDLEY_DEPRECATED_FOR(3.11.2, operator==(json_pointer))
    bool operator==(const string_t& rhs) const
    {
        return *this == json_pointer(rhs);
    }

    /// @brief 3-way compares two JSON pointers
    template<typename RefStringTypeRhs>
    std::strong_ordering operator<=>(const json_pointer<RefStringTypeRhs>& rhs) const noexcept // *NOPAD*
    {
        return  reference_tokens <=> rhs.reference_tokens; // *NOPAD*
    }
#else
    /// @brief compares two JSON pointers for equality
    /// @sa https://json.nlohmann.me/api/json_pointer/operator_eq/
    template<typename RefStringTypeLhs, typename RefStringTypeRhs>
    // NOLINTNEXTLINE(readability-redundant-declaration)
    friend bool operator==(const json_pointer<RefStringTypeLhs>& lhs,
                           const json_pointer<RefStringTypeRhs>& rhs) noexcept;

    /// @brief compares JSON pointer and string for equality
    /// @sa https://json.nlohmann.me/api/json_pointer/operator_eq/
    template<typename RefStringTypeLhs, typename StringType>
    // NOLINTNEXTLINE(readability-redundant-declaration)
    friend bool operator==(const json_pointer<RefStringTypeLhs>& lhs,
                           const StringType& rhs);

    /// @brief compares string and JSON pointer for equality
    /// @sa https://json.nlohmann.me/api/json_pointer/operator_eq/
    template<typename RefStringTypeRhs, typename StringType>
    // NOLINTNEXTLINE(readability-redundant-declaration)
    friend bool operator==(const StringType& lhs,
                           const json_pointer<RefStringTypeRhs>& rhs);

    /// @brief compares two JSON pointers for inequality
    /// @sa https://json.nlohmann.me/api/json_pointer/operator_ne/
    template<typename RefStringTypeLhs, typename RefStringTypeRhs>
    // NOLINTNEXTLINE(readability-redundant-declaration)
    friend bool operator!=(const json_pointer<RefStringTypeLhs>& lhs,
                           const json_pointer<RefStringTypeRhs>& rhs) noexcept;

    /// @brief compares JSON pointer and string for inequality
    /// @sa https://json.nlohmann.me/api/json_pointer/operator_ne/
    template<typename RefStringTypeLhs, typename StringType>
    // NOLINTNEXTLINE(readability-redundant-declaration)
    friend bool operator!=(const json_pointer<RefStringTypeLhs>& lhs,
                           const StringType& rhs);

    /// @brief compares string and JSON pointer for inequality
    /// @sa https://json.nlohmann.me/api/json_pointer/operator_ne/
    template<typename RefStringTypeRhs, typename StringType>
    // NOLINTNEXTLINE(readability-redundant-declaration)
    friend bool operator!=(const StringType& lhs,
                           const json_pointer<RefStringTypeRhs>& rhs);

    /// @brief compares two JSON pointer for less-than
    template<typename RefStringTypeLhs, typename RefStringTypeRhs>
    // NOLINTNEXTLINE(readability-redundant-declaration)
    friend bool operator<(const json_pointer<RefStringTypeLhs>& lhs,
                          const json_pointer<RefStringTypeRhs>& rhs) noexcept;
#endif

  private:
    /// the reference tokens
    std::vector<string_t> reference_tokens;
};

#if !JSON_HAS_THREE_WAY_COMPARISON
// functions cannot be defined inside class due to ODR violations
template<typename RefStringTypeLhs, typename RefStringTypeRhs>
inline bool operator==(const json_pointer<RefStringTypeLhs>& lhs,
                       const json_pointer<RefStringTypeRhs>& rhs) noexcept
{
    return lhs.reference_tokens == rhs.reference_tokens;
}

template<typename RefStringTypeLhs,
         typename StringType = typename json_pointer<RefStringTypeLhs>::string_t>
JSON_HEDLEY_DEPRECATED_FOR(3.11.2, operator==(json_pointer, json_pointer))
inline bool operator==(const json_pointer<RefStringTypeLhs>& lhs,
                       const StringType& rhs)
{
    return lhs == json_pointer<RefStringTypeLhs>(rhs);
}

template<typename RefStringTypeRhs,
         typename StringType = typename json_pointer<RefStringTypeRhs>::string_t>
JSON_HEDLEY_DEPRECATED_FOR(3.11.2, operator==(json_pointer, json_pointer))
inline bool operator==(const StringType& lhs,
                       const json_pointer<RefStringTypeRhs>& rhs)
{
    return json_pointer<RefStringTypeRhs>(lhs) == rhs;
}

template<typename RefStringTypeLhs, typename RefStringTypeRhs>
inline bool operator!=(const json_pointer<RefStringTypeLhs>& lhs,
                       const json_pointer<RefStringTypeRhs>& rhs) noexcept
{
    return !(lhs == rhs);
}

template<typename RefStringTypeLhs,
         typename StringType = typename json_pointer<RefStringTypeLhs>::string_t>
JSON_HEDLEY_DEPRECATED_FOR(3.11.2, operator!=(json_pointer, json_pointer))
inline bool operator!=(const json_pointer<RefStringTypeLhs>& lhs,
                       const StringType& rhs)
{
    return !(lhs == rhs);
}

template<typename RefStringTypeRhs,
         typename StringType = typename json_pointer<RefStringTypeRhs>::string_t>
JSON_HEDLEY_DEPRECATED_FOR(3.11.2, operator!=(json_pointer, json_pointer))
inline bool operator!=(const StringType& lhs,
                       const json_pointer<RefStringTypeRhs>& rhs)
{
    return !(lhs == rhs);
}

template<typename RefStringTypeLhs, typename RefStringTypeRhs>
inline bool operator<(const json_pointer<RefStringTypeLhs>& lhs,
                      const json_pointer<RefStringTypeRhs>& rhs) noexcept
{
    return lhs.reference_tokens < rhs.reference_tokens;
}
#endif

NLOHMANN_JSON_NAMESPACE_END
