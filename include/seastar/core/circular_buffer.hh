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

#include <seastar/core/transfer.hh>
#include <seastar/core/bitops.hh>
#include <memory>
#include <algorithm>

namespace seastar {
    /// A growable double-ended queue container that can be efficiently
    /// extended (and shrunk) from both ends. Implementation is a single
    /// storage vector.
    ///
    /// Similar to libstdc++'s std::deque, except that it uses a single
    /// level store, and so is more efficient for simple stored items.
    /// Similar to boost::circular_buffer_space_optimized, except it uses
    /// uninitialized storage for unoccupied elements (and thus move/copy
    /// constructors instead of move/copy assignments, which are less
    /// efficient).
    ///
    /// The storage of the circular_buffer is expanded automatically in
    /// exponential increments.
    /// When adding new elements:
    /// * if size + 1 > capacity: all iterators and references are
    ///     invalidated,
    /// * otherwise only the begin() or end() iterator is invalidated:
    ///     * push_front() and emplace_front() will invalidate begin() and
    ///     * push_back() and emplace_back() will invalidate end().
    ///
    /// Removing elements never invalidates any references and only
    /// invalidates begin() or end() iterators:
    ///     * pop_front() will invalidate begin() and
    ///     * pop_back() will invalidate end().
    ///
    /// reserve() may also invalidate all iterators and references.
    template <typename T>
    class circular_buffer {
        size_t _begin{0};
        size_t _end{0};
        size_t _capacity{0};

        // struct impl : Alloc {
        //     T* storage = nullptr;
        //     // begin, end interpreted (mod capacity)
        //     size_t begin = 0;
        //     size_t end = 0;
        //     size_t capacity = 0;
        // };
        std::allocator<T> _alloc{};
        T *_impl{nullptr};
        using traits = std::allocator_traits<decltype(_alloc)>;
    public:
        using value_type = T;
        using size_type = size_t;
        using reference = T&;
        using pointer = T*;
        using const_reference = const T&;
        using const_pointer = const T*;
    public:
        circular_buffer() = default;
        circular_buffer(circular_buffer&& X) noexcept;
        circular_buffer(const circular_buffer& X) = delete;
        ~circular_buffer();
        circular_buffer& operator=(const circular_buffer&) = delete;
        circular_buffer& operator=(circular_buffer&& b) noexcept;
        void push_front(const T& data);
        void push_front(T&& data);
        template <typename... A>
        void emplace_front(A&&... args);
        void push_back(const T& data);
        void push_back(T&& data);
        template <typename... A>
        void emplace_back(A&&... args);
        T& front();
        const T& front() const;
        T& back();
        const T& back() const;
        void pop_front();
        void pop_back();
        bool empty() const;
        size_t size() const;
        size_t capacity() const;
        void reserve(size_t);
        void clear();
        T& operator[](size_t idx);
        // const T& operator[](size_t idx) const;
        template <typename Func>
        void for_each(Func&& func);
        // access an element, may return wrong or destroyed element
        // only useful if you do not rely on data accuracy (e.g. prefetch)
        T& access_element_unsafe(size_t idx);
    private:
        void expand();
        void realloc(size_t);
        void maybe_expand(size_t nr = 1);
        size_t mask(size_t idx) const;

        struct Iterator {
            T *operator->() const noexcept { 
                return std::addressof(cb->at(idx)); 
            }
            
            T& operator*() const noexcept { 
                return cb->at(idx); 
            }

            // ValueType& operator*() const { return cb[idx]; }
            // ValueType* operator->() const { return &cb[idx]; }

            // prefix
            Iterator& operator++() noexcept {
                ++idx;
                return *this;
            }
            
            // postfix
            Iterator operator++(int unused) noexcept {
                auto v = *this;
                ++idx;
                return v;
            }
            
            // prefix
            Iterator& operator--() noexcept {
                --idx;
                return *this;
            }
            
            // postfix
            Iterator operator--(int unused) noexcept {
                auto v = *this;
                --idx;
                return v;
            }
            
            Iterator operator+(size_t n) noexcept {
                return {cb, idx + n};
            }
            
            Iterator operator-(size_t n) noexcept {
                return {cb, idx - n};
            }
            
            Iterator& operator+=(size_t n) noexcept {
                idx += n;
                return *this;
            }
            
            Iterator& operator-=(size_t n) noexcept {
                idx -= n;
                return *this;
            }
            
            bool operator==(Iterator rhs) const noexcept {
                return idx == rhs.idx;
            }
            
            bool operator!=(Iterator rhs) const noexcept {
                return idx != rhs.idx;
            }
            
            bool operator<(Iterator rhs) const noexcept {
                return idx < rhs.idx;
            }
            
            bool operator>(Iterator rhs) const noexcept {
                return idx > rhs.idx;
            }
            
            bool operator>=(Iterator rhs) const noexcept {
                return idx >= rhs.idx;
            }
            
            bool operator<=(Iterator rhs) const noexcept {
                return idx <= rhs.idx;
            }

            size_t operator-(Iterator rhs) const noexcept {
                return idx - rhs.idx;
            }

            Iterator(circular_buffer<T> *cb, size_t idx) noexcept : cb{cb}, idx{idx} {}

        private:
            circular_buffer<T>* cb;
            size_t idx;
            // friend class circular_buffer;
        };
        // friend class iterator;

    public:
        using iterator = Iterator;
        using const_iterator = const iterator;

        iterator begin () noexcept {
            return {this, _begin};
        }

        iterator end () noexcept {
            return {this, _end};
        }

        const_iterator cbegin () const noexcept {
            return const_iterator{this, _begin};
        }
        const_iterator cend () const noexcept {
            return const_iterator{this, _end};
        }

        inline T& at (size_t idx) {
            return _impl[mask(_begin + idx)];
        }
        iterator erase (iterator first, iterator last);
    };

    // template <typename T>
    // inline
    // const T&
    // circular_buffer<T>::operator[](size_t idx) const {
    //     return _impl[mask(_begin + idx)];
    // }
};
