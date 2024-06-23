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
        std::size_t _begin{0};
        std::size_t _end{0};
        std::size_t _capacity{0};

        // struct impl : Alloc {
        //     T* storage = nullptr;
        //     // begin, end interpreted (mod capacity)
        //     std::size_t begin = 0;
        //     std::size_t end = 0;
        //     std::size_t capacity = 0;
        // };
        std::allocator<T> _alloc{};
        T *_impl{nullptr};
        using traits = std::allocator_traits<decltype(_alloc)>;
    public:
        using value_type = T;
        using size_type = std::size_t;
        using reference = T&;
        using pointer = T*;
        using const_reference = const T&;
        using const_pointer = const T*;

        circular_buffer() = default;

        inline circular_buffer(circular_buffer&& x) noexcept : _impl(std::move(x._impl)), _begin{x._begin}, _end{x._end}, _capacity{x._capacity} {
            x._impl = nullptr;
            x._begin = 0;
            x._end = 0;
            x._capacity = 0;
        }

        inline ~circular_buffer() {
            if (_impl != nullptr) {
                for_each([] (T& obj) {
                    std::destroy_at(std::addressof(obj));
                });
                traits::deallocate(_alloc, _impl, _capacity);
            }
        }
    private:
        inline std::size_t mask(std::size_t idx) const {
            return idx & (_capacity - 1);
        }

        inline void maybe_expand(std::size_t nr = 1) {
            printf("maybe_expand: %lu\n", nr);
            printf("testing\n");
            if (_capacity == 0) {
                printf("asdf\n");
            }
            if (_begin == 0) {
                printf("fdsa\n");
            }
            printf("test: %lu\n", _capacity);

            if ((_end - _begin) + nr > _capacity) {
                printf("calling expand\n");
                expand();
            }
        }

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

            Iterator(circular_buffer<T> *cb, std::size_t idx) noexcept : cb{cb}, idx{idx} {}

        private:
            circular_buffer<T>* cb;
            std::size_t idx;
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

        inline T& at (std::size_t idx) {
            return _impl[mask(_begin + idx)];
        }

        inline bool empty() const {
            return _begin == _end;
        }

        inline std::size_t size() const {
            return _end - _begin;
        }

        inline std::size_t capacity() const {
            return _capacity;
        }

        inline void reserve(std::size_t size) {
            if (capacity() < size) {
                // Make sure that the new capacity is a power of two.
                realloc(size_t(1) << log2ceil(size));
            }
        }

        inline void clear() {
            erase(begin(), end());
        }

        inline T& front() {
            return _impl[mask(_begin)];
        }

        inline const T& front() const {
            return _impl[mask(_begin)];
        }

        inline T& back() {
            return _impl[mask(_end - 1)];
        }

        inline const T& back() const {
            return _impl[mask(_end - 1)];
        }

        inline void pop_front() {
            std::destroy_at(std::addressof(front()));
            ++_begin;
        }

        inline void pop_back() {
            std::destroy_at(std::addressof(back()));
            --_end;
        }

        inline T& operator[](std::size_t idx) {
            return _impl[mask(_begin + idx)];
        }

        inline circular_buffer<T>& operator=(circular_buffer&& x) noexcept {
            if (this != &x) {
                this->~circular_buffer();
                new (this) circular_buffer(std::move(x));
            }
            return *this;
        }

        template <typename Func>
        inline void for_each(Func&& func) {
            for (auto&& p{begin()}, e{end()}; p != e; ++p) {
                func(*p);
            }
        }

        inline void push_front(const T& data) {
            maybe_expand();
            --_begin;
            std::construct_at(std::addressof(_impl[_begin]), data);
        }

        inline void push_front(T&& data) {
            maybe_expand();
            --_begin;
            std::construct_at(std::addressof(_impl[mask(_begin)]), std::move(data));
        }

        template <typename... Args>
        inline void emplace_front(Args&&... args) {
            maybe_expand();
            --_begin;
            std::construct_at(std::addressof(_impl[mask(_begin)]), std::forward<Args>(args)...);
        }

        inline void push_back(const T& data) {
            printf("circular_buffer const push_back\n");
            maybe_expand();
            printf("circular_buffer copy maybe expanded\n");
            std::construct_at(std::addressof(_impl[_end]), data);
            printf("circular_buffer copy constructed\n");        
            ++_end;
        }

        inline void push_back(T&& data) {
            printf("circular_buffer move push_back: 0x%lx\n", (uint64_t)this);
            maybe_expand();
            printf("circular_buffer move maybe expanded\n");
            printf("circular_buffer move constructing...\n");        
            std::construct_at(std::addressof(_impl[_end]), std::move(data));
            printf("circular_buffer move constructed\n");
            ++_end;
        }

        template <typename... Args>
        inline void emplace_back(Args&&... args) {
            maybe_expand();
            std::construct_at(std::addressof(_impl[_end]), std::forward<Args>(args)...);
            ++_end;
        }

        inline T& access_element_unsafe(std::size_t idx) {
            return _impl[mask(_begin + idx)];
        }

        inline iterator erase(iterator first, iterator last) {
            static_assert(std::is_nothrow_move_assignable<T>::value, "erase() assumes move assignment does not throw");
            if (first == last) {
                return last;
            }
            // Move to the left or right depending on which would result in least amount of moves.
            // This also guarantees that iterators will be stable when removing from either front or back.
            if (std::distance(begin(), first) < std::distance(last, end())) {
                auto new_start = std::move_backward(begin(), first, last);
                for (auto i = begin(); i < new_start; ++i) {
                    traits::destroy(_impl, &*i);
                }

                _begin = new_start.idx;
                return last;
            } else {
                auto new_end = std::move(last);
                for (auto i = new_end, e = end(); i < e; ++i) {
                    traits::destroy(_impl, this[i]);
                }

                _end = new_end.idx;
                return first;
            }
        }

        void expand() {
            reserve(std::max<std::size_t>(_capacity * 2, 1));
        }

        void realloc(std::size_t new_cap) {
            printf("expand: %lu\n", new_cap);
            T *new_storage{traits::allocate(_alloc, new_cap)};
            T *p{new_storage};

            try {
                printf("for_each transfer_pass1\n");
                for_each([this, &p] (T& obj) {
                    transfer_pass1(_alloc, std::addressof(obj), p);
                    p++;
                });
                printf("first transfer\n");
            } catch (...) {
                printf("exceptions encountered\n");
                while (p != new_storage) {
                    std::destroy_at(--p);
                }
                traits::deallocate(_alloc, new_storage, new_cap);
                throw;
            }
            p = new_storage;
            printf("start transfer_pass2\n");
            for_each([this, &p] (T& obj) {
                transfer_pass2(_alloc, std::addressof(obj), p++);
            });
            printf("finish transfer_pass2\n");
            std::swap(_impl, new_storage);
            std::swap(_capacity, new_cap);
            printf("deallocating\n");
            traits::deallocate(_alloc, new_storage, new_cap);
            printf("expanded\n");
        }
    };

    // template <typename T>
    // inline
    // const T&
    // circular_buffer<T>::operator[](std::size_t idx) const {
    //     return _impl[mask(_begin + idx)];
    // }
};
