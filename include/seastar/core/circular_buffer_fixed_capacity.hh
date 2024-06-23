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
 * Copyright (C) 2017 ScyllaDB
 */

#pragma once

// A fixed capacity double-ended queue container that can be efficiently
// extended (and shrunk) from both ends.  Implementation is a single
// storage vector.
//
// Similar to libstdc++'s std::deque, except that it uses a single level
// store, and so is more efficient for simple stored items.

#include <type_traits>
#include <cstddef>
#include <iterator>
#include <utility>


/// \file

namespace seastar {
    /// A fixed-capacity container (like boost::static_vector) that can insert
    /// and remove at both ends (like std::deque). Does not allocate.
    ///
    /// Does not perform overflow checking when size exceeds capacity.
    ///
    /// \tparam T type of objects stored in the container; must be noexcept move enabled
    /// \tparam Capacity maximum number of objects that can be stored in the container; must be a power of 2
    template <typename T, size_t Capacity>
    class circular_buffer_fixed_capacity {
        size_t _begin = 0;
        size_t _end = 0;
        union maybe_storage {
            T data;
            maybe_storage() noexcept {}
            ~maybe_storage() {}
        };
        maybe_storage _storage[Capacity];
    private:
        static size_t mask(size_t idx) { return idx % Capacity; }
        T* obj(size_t idx) { return &_storage[mask(idx)].data; }
        const T* obj(size_t idx) const { return &_storage[mask(idx)].data; }
    public:
        static_assert((Capacity & (Capacity - 1)) == 0, "capacity must be a power of two");
        static_assert(std::is_nothrow_move_constructible<T>::value && std::is_nothrow_move_assignable<T>::value,
                "circular_buffer_fixed_capacity only supports nothrow-move value types");
        using value_type = T;
        using size_type = size_t;
        using reference = T&;
        using pointer = T*;
        using const_reference = const T&;
        using const_pointer = const T*;
        using difference_type = ssize_t;
    public:
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
            
            Iterator operator+(std::size_t n) noexcept {
                return {cb, idx + n};
            }
            
            Iterator operator-(std::size_t n) noexcept {
                return {cb, idx - n};
            }
            
            Iterator& operator+=(std::size_t n) noexcept {
                idx += n;
                return *this;
            }
            
            Iterator& operator-=(std::size_t n) noexcept {
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

            std::size_t operator-(Iterator rhs) const noexcept {
                return idx - rhs.idx;
            }

            Iterator(circular_buffer_fixed_capacity<T, Capacity> *cb, std::size_t idx) noexcept : cb{cb}, idx{idx} {}

        private:
            circular_buffer_fixed_capacity<T, Capacity>* cb;
            std::size_t idx;
        };
    public:
        using iterator = Iterator;
        using const_iterator = const Iterator;
    public:
        circular_buffer_fixed_capacity() = default;
        inline circular_buffer_fixed_capacity(circular_buffer_fixed_capacity&& x) noexcept : _begin(std::exchange(x._begin, 0)), _end(std::exchange(x._end, 0)) {
            for (auto i = _begin; i != _end; ++i) {
                new (&_storage[i].data) T(std::move(x._storage[i].data));
            }
        }

        inline ~circular_buffer_fixed_capacity() noexcept {
            for (auto i = _begin; i != _end; ++i) {
                _storage[i].data.~T();
            }
        }

        template <typename... Args>
        inline T& emplace_back(Args&&... args) {
            auto p = new (obj(_end)) T(std::forward<Args>(args)...);
            ++_end;
            return *p;
        }

        template <typename... Args>
        inline T& emplace_front(Args&&... args) {
            auto p = new (obj(_begin - 1)) T(std::forward<Args>(args)...);
            --_begin;
            return *p;
        }

        inline T& front() {
            return *obj(_begin);
        }

        inline T& back() {
            return *obj(_end - 1);
        }

        inline void pop_front() {
            obj(_begin)->~T();
            ++_begin;
        }

        inline void pop_back() {
            obj(_end - 1)->~T();
            --_end;
        }

        inline circular_buffer_fixed_capacity<T, Capacity>& operator=(circular_buffer_fixed_capacity&& x) noexcept {
            if (this != &x) {
                this->~circular_buffer_fixed_capacity();
                new (this) circular_buffer_fixed_capacity(std::move(x));
            }
            return *this;
        }

        inline void push_front(const T& data) {
            new (obj(_begin - 1)) T(data);
            --_begin;
        }

        inline void push_front(T&& data) {
            new (obj(_begin - 1)) T(std::move(data));
            --_begin;
        }

        inline void push_back(const T& data) {
            printf("fixed capacity push_back copy\n");
            new (obj(_end)) T(data);
            printf("pushed back copy\n");
            ++_end;
        }

        inline void push_back(T&& data) {
            printf("fixed capacity push_back move\n");
            new (obj(_end)) T(std::move(data));
            printf("pushed back move\n");
            ++_end;
        }

        inline bool empty() const noexcept {
            return _begin == _end;
        }

        inline std::size_t size() const noexcept {
            return (_end - _begin) % Capacity;
        }

        inline std::size_t constexpr capacity () const noexcept {
            return Capacity;
        }

        inline T& operator[](std::size_t idx) noexcept {
            return *obj(_begin + idx);
        }

        inline T& at (std::size_t idx) noexcept {
            return *obj(_begin + idx);
        }

        inline iterator begin() {
            return {this, _begin};
        }
        
        inline const_iterator begin() const {
            return {this, _begin};
        }
        
        inline iterator end() {
            return {this, _end};
        }
        
        inline const_iterator end() const {
            return {this, _end};
        }
        
        inline const_iterator cbegin() const {
            return {this, _begin};
        }
        
        inline const_iterator cend() const {
            return {this, _end};
        }

        inline iterator erase (iterator first, iterator last) {
            static_assert(std::is_nothrow_move_assignable<T>::value, "erase() assumes move assignment does not throw");
            if (first == last) {
                return last;
            }
            // Move to the left or right depending on which would result in least amount of moves.
            // This also guarantees that iterators will be stable when removing from either front or back.
            if (std::distance(begin(), first) < std::distance(last, end())) {
                auto new_start = std::move_backward(begin(), first, last);
                auto i = begin();
                while (i < new_start) {
                    *i++.~T();
                }
                _begin = new_start.idx;
                return last;
            } else {
                auto new_end = std::move(last, end(), first);
                auto i = new_end;
                auto e = end();
                while (i < e) {
                    *i++.~T();
                }
                _end = new_end.idx;
                return first;
            }
        }

        inline void clear () {
            for (auto i = _begin; i != _end; ++i) {
                obj(i)->~T();
            }
            _begin = _end = 0;
        }
    };
};

