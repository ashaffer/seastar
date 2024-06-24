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
    template <typename T, std::size_t Capacity>
    class circular_buffer_fixed_capacity {
        std::size_t head{0};
        std::size_t tail{0};

        union maybe_storage {
            T data;
            maybe_storage () noexcept {}
            ~maybe_storage () noexcept {}
        };
        maybe_storage _storage[Capacity];
    public:
        static_assert((Capacity & (Capacity - 1)) == 0, "capacity must be a power of two");
        static_assert(std::is_nothrow_move_constructible<T>::value && std::is_nothrow_move_assignable<T>::value, "circular_buffer_fixed_capacity only supports nothrow-move value types");
        using value_type = T;
        using size_type = std::size_t;
        using reference = T&;
        using pointer = T*;
        using const_reference = const T&;
        using const_pointer = const T*;
        using difference_type = ssize_t;

    private:
        struct Iterator {
            T *operator-> () const noexcept { 
                return std::addressof(cb->at(idx)); 
            }
            
            T& operator* () const noexcept { 
                return cb->at(idx); 
            }

            // ValueType& operator*() const { return cb[idx]; }
            // ValueType* operator->() const { return &cb[idx]; }

            // prefix
            Iterator& operator++ () noexcept {
                ++idx;
                if (idx == Capacity) {
                    idx = 0;
                }
                return *this;
            }
            
            // postfix
            Iterator operator++ (int unused) noexcept {
                auto v = *this;
                ++idx;
                if (idx == Capacity) {
                    idx = 0;
                }
                return v;
            }
            
            // prefix
            Iterator& operator-- () noexcept {
                if (idx == 0) {
                    idx = Capacity - 1;
                } else {
                    --idx;
                }
                return *this;
            }
            
            // postfix
            Iterator operator-- (int unused) noexcept {
                auto v = *this;
                if (idx == 0) {
                    idx = Capacity - 1;
                } else {
                    --idx;
                }
                return v;
            }
            
            Iterator operator+ (std::size_t n) noexcept {
                return {cb, idx + n};
            }
            
            Iterator operator- (std::size_t n) noexcept {
                return {cb, idx - n};
            }
            
            Iterator& operator+= (std::size_t n) noexcept {
                idx += n;
                if (idx > Capacity) {
                    idx %= Capacity;
                }
                return *this;
            }
            
            Iterator& operator-= (std::size_t n) noexcept {
                idx = idx > n ? idx - n : (Capacity - (idx - n));
                return *this;
            }
            
            bool operator== (Iterator rhs) const noexcept {
                return idx == rhs.idx;
            }
            
            bool operator!= (Iterator rhs) const noexcept {
                return idx != rhs.idx;
            }
            
            bool operator< (Iterator rhs) const noexcept {
                return idx < rhs.idx;
            }
            
            bool operator> (Iterator rhs) const noexcept {
                return idx > rhs.idx;
            }
            
            bool operator>= (Iterator rhs) const noexcept {
                return idx >= rhs.idx;
            }
            
            bool operator<=(Iterator rhs) const noexcept {
                return idx <= rhs.idx;
            }

            std::size_t operator- (Iterator rhs) const noexcept {
                return idx - rhs.idx;
            }

            Iterator (circular_buffer_fixed_capacity<T, Capacity> *cb, std::size_t idx) noexcept : cb{cb}, idx{idx} {}

            circular_buffer_fixed_capacity<T, Capacity>* cb;
            std::size_t idx;
            friend class circular_buffer_fixed_capacity;
        };

        template<bool bump_head>
        T *advance_tail () noexcept {
            T *t{std::addressof(_storage[tail].data)};

            if (++tail == Capacity) {
                tail = 0;
            }

            if (tail == head) {
                t->~T();
                if constexpr (bump_head) {
                    if (++head == Capacity) {
                        head = 0;
                    }
                }
            }

            return t;
        }

        template<bool bump_tail>
        T *advance_head () noexcept {
            T *t{std::addressof(_storage[head].data)};

            if (++head == Capacity) {
                head = 0;
            }

            if (head == tail) {
                if constexpr (bump_tail) {
                    if (++tail == Capacity) {
                        tail = 0;
                    }
                }
                t->~T();
            }

            return t;
        }

        template<bool bump_head>
        T *dec_tail () noexcept {
            if (tail-- == 0) {
                tail = Capacity - 1;
            }

            T *t{std::addressof(_storage[tail].data)};
            if (tail == head) {
                if constexpr (bump_head) {
                    if (head-- == 0) {
                        head = Capacity - 1;
                    }                    
                }
                t->~T();
            }

            return t;
        }

        T *dec_head () noexcept {
            T *t{std::addressof(_storage[head].data)};

            if (head-- == 0) {
                head = Capacity - 1;
            }

            if (head == tail) {
                if (tail-- == 0) {
                    tail = Capacity - 1;
                }
                t->~T();
            }

            return t;
        }
    public:
        using iterator = Iterator;
        using const_iterator = const Iterator;

        circular_buffer_fixed_capacity () noexcept = default;
        inline circular_buffer_fixed_capacity (circular_buffer_fixed_capacity&& x) noexcept : head(std::exchange(x.head, 0)), tail(std::exchange(x.tail, 0)) {
            std::size_t i{head};

            for (auto&& it : *this) {
                new (std::addressof(it)) T(std::move(x._storage[i].data));
                ++i;
            }
        }

        inline ~circular_buffer_fixed_capacity () noexcept {
            for (auto&& it : *this) {
                it.~T();
            }
        }

        template <typename... Args>
        inline T& emplace_back (Args&&... args) noexcept {
            T *t{advance_tail<true>()};
            new (t)T{std::forward<Args>(args)...};
            return *t;
        }

        template <typename... Args>
        inline T& emplace_front (Args&&... args) noexcept {
            T *t{advance_head<true>()};
            new (t) T(std::forward<Args>(args)...);
            return *t;
        }

        inline void push_front (const T& data) noexcept {
            T *t{advance_head<true>()};
            new (t) T(data);
        }

        inline void push_front (T&& data) noexcept {
            T *t{advance_head<true>()};
            new (t) T(std::move(data));
        }

        inline void push_back (const T& data) noexcept {
            T *t{advance_tail<true>()};
            new (t) T(data);
        }

        inline void push_back (T&& data) noexcept {
            T *t{advance_tail<true>()};
            new (t) T(std::move(data));
        }

        inline void pop_front () noexcept {
            advance_head<false>();
        }

        inline void pop_back () noexcept {
            dec_tail<false>();
        }

        inline T& front () noexcept {
            return _storage[head].data;
        }

        inline T& back () noexcept {
            return _storage[tail].data;
        }

        inline circular_buffer_fixed_capacity& operator= (circular_buffer_fixed_capacity&& x) noexcept {
            if (this != &x) {
                this->~circular_buffer_fixed_capacity();
                new (this) circular_buffer_fixed_capacity(std::move(x));
            }
            return *this;
        }

        inline bool empty () const noexcept {
            return head == tail;
        }

        inline std::size_t size () const noexcept {
            return head < tail
                ? tail - head
                : (Capacity - head) + tail;
        }

        inline std::size_t constexpr capacity () const noexcept {
            return Capacity;
        }

        inline T& operator[](std::size_t idx) noexcept {
            return _storage[idx].data;
        }

        inline T& at (std::size_t idx) noexcept {
            return _storage[idx].data;
        }

        inline iterator begin () {
            return {this, head};
        }
        
        inline const_iterator begin () const noexcept {
            return {this, head};
        }
        
        inline iterator end () noexcept {
            return {this, tail};
        }
        
        inline const_iterator end () const noexcept {
            return {this, tail};
        }
        
        inline const_iterator cbegin () const noexcept {
            return {this, head};
        }
        
        inline const_iterator cend () const noexcept {
            return {this, tail};
        }

        inline iterator erase (iterator first, iterator last) noexcept {
            static_assert(std::is_nothrow_move_assignable<T>::value, "erase() assumes move assignment does not throw");

            while (first != last) {
                ++first;
            }

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
                head = new_start.idx;
                return last;
            } else {
                auto new_end = std::move(last, end(), first);
                auto i = new_end;
                auto e = end();
                while (i < e) {
                    *i++.~T();
                }
                head = new_end.idx;
                return first;
            }
        }

        inline void clear () {
            while (!empty()) {
                pop_front();
            }
        }
    };
};

