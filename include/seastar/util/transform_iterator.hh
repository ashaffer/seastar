/*
 * This file is open source software, licensed to you under the terms
 * of the Apache License, Version 2.0 (the "License").  See the NOTICE file
 * distributed with this work for additional information regarding copyright
 * ownership.  You may not use this file except in compliance with the License.
 *
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
/*
 * Copyright (C) 2014 Cloudius Systems, Ltd.
 */

#pragma once
#include <iterator>
#include <string_view>
#include <cxxabi.h>
#include <concepts>
#include <type_traits>
#include <array>
#include <utility>
#include <print>

namespace seastar {
    template<std::input_iterator Iter, std::invocable<typename std::iter_value_t<Iter>> Func>
    class transform_iterator {
        Iter it;
        Func fn;

        using src_type = std::iter_value_t<Iter>;
        // using src_category = std::enable_if_t<std::void_t<typename Iter::iterator_category, std::input_iterator_tag>>;
        // using src_concept = std::enable_if_t<std::void_t<typename Iter::iterator_category, std::input_iterator_tag>>;
    public:
        using iterator_category = std::input_iterator_tag;
        using value_type = std::invoke_result_t<Func, src_type>;
        using reference = std::add_lvalue_reference_t<std::remove_reference_t<value_type>>;
        using const_reference = std::add_lvalue_reference_t<std::add_const_t<std::remove_cvref_t<value_type>>>;
        using rvalue_reference = std::add_rvalue_reference_t<std::remove_cvref_t<value_type>>;
        using pointer = void;
        using const_pointer = void;
        // using iterator_category = src_category;
        
        transform_iterator() = delete;
        transform_iterator (const Iter& it, Func fn) noexcept : it{it}, fn{fn} {}
        transform_iterator (Iter&& it, Func fn) noexcept : it{std::move(it)}, fn{fn} {}

        // transform_iterator (const Iter& it, std::same_as<Func> auto&& fn) noexcept : fn{std::forward<decltype(fn)>(fn)}, it{it} {

        // }

        decltype(auto) operator++ () {
            ++it;
            return *this;
        }

        decltype(auto) operator++ (int) {
            auto tmp = *this;
            ++it;
            return tmp;
        }

        bool operator== (const transform_iterator& other) const noexcept {
            return other.it == it;
        }

        bool operator!= (const transform_iterator& other) const noexcept {
            return it != other.it;
        }

        decltype(auto) operator* () noexcept {
            return fn(*it);
        }

        decltype(auto) operator* () const noexcept {
            return fn(*it);
        }
    };


    template<class View, std::invocable<typename std::iter_value_t<View>> Func>
    class transform_view {
        View& view;
        Func fn;    
    public:
        transform_view (View& view, Func fn) noexcept : view{view}, fn{fn} {}
        transform_view (View&& view, Func&& fn) noexcept : view{std::move(view)}, fn{std::move(fn)} {}

        // transform_view (const View& view, Func fn) noexcept : view{view}, fn{fn} {}


        decltype(auto) begin () noexcept {
            return transform_iterator(std::begin(view), fn);
        }

        decltype(auto) cbegin () const noexcept {
            return transform_iterator(std::cbegin(view), fn);
        }

        decltype(auto) end () noexcept {
            return transform_iterator{std::end(view), fn};
        }

        decltype(auto) cend () const noexcept {
            return transform_iterator{std::cend(view), fn};
        }
    };

    template<std::size_t N>
    struct extract_nth {
        decltype(auto) operator() (auto&& it) {
            return std::get<N>(std::forward<decltype(it)>(it));
        }
    };
};