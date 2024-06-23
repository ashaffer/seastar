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

        inline size_t mask(size_t idx) const {
            return idx & (_capacity - 1);
        }

        inline bool empty() const {
            return _begin == _end;
        }

        inline size_t size() const {
            return _end - _begin;
        }

        inline size_t capacity() const {
            return _capacity;
        }

        inline void reserve(size_t size) {
            if (capacity() < size) {
                // Make sure that the new capacity is a power of two.
                realloc(size_t(1) << log2ceil(size));
            }
        }

        inline void clear() {
            erase(begin(), end());
        }

        inline circular_buffer(circular_buffer&& x) noexcept : _impl(std::move(x._impl)), _begin{x._begin}, _end{x._end}, _capacity{x._capacity} {
            x._impl = nullptr;
            x._begin = 0;
            x._end = 0;
            x._capacity = 0;
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

        inline T& operator[](size_t idx) {
            return _impl[mask(_begin + idx)];
        }

        inline circular_buffer<T>&  operator=(circular_buffer&& x) noexcept {
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

        iterator erase (iterator first, iterator last);
    };

    // template <typename T>
    // inline
    // const T&
    // circular_buffer<T>::operator[](size_t idx) const {
    //     return _impl[mask(_begin + idx)];
    // }

    template <typename T>
    inline circular_buffer<T>::~circular_buffer() {
        if (_impl != nullptr) {
            for_each([] (T& obj) {
                std::destroy_at(std::addressof(obj));
            });
            traits::deallocate(_alloc, _impl, _capacity);
        }
    }

    template <typename T>
    inline void circular_buffer<T>::maybe_expand(size_t nr) {
        printf("test: %lu\n", _capacity);
        printf("maybe_expand: %lu\n", nr);
        if ((_end - _begin) + nr > _capacity) {
            printf("calling expand\n");
            expand();
        }
    }

    template <typename T>
    inline void circular_buffer<T>::push_front(const T& data) {
        maybe_expand();
        --_begin;
        std::construct_at(std::addressof(_impl[_begin]), data);
    }

    template <typename T>
    inline void circular_buffer<T>::push_front(T&& data) {
        maybe_expand();
        --_begin;
        std::construct_at(std::addressof(_impl[mask(_begin)]), std::move(data));
    }

    template <typename T>
    template <typename... Args>
    inline void circular_buffer<T>::emplace_front(Args&&... args) {
        maybe_expand();
        --_begin;
        std::construct_at(std::addressof(_impl[mask(_begin )]), std::forward<Args>(args)...);
    }

    template <typename T>
    inline void circular_buffer<T>::push_back(const T& data) {
        printf("circular_buffer const push_back\n");
        maybe_expand();
        printf("circular_buffer copy maybe expanded\n");
        std::construct_at(std::addressof(_impl[_end]), data);
        printf("circular_buffer copy constructed\n");        
        ++_end;
    }

    template <typename T>
    inline void circular_buffer<T>::push_back(T&& data) {
        printf("circular_buffer move push_back\n");
        maybe_expand();
        printf("circular_buffer move maybe expanded\n");
        printf("circular_buffer move constructing...\n");        
        std::construct_at(std::addressof(_impl[_end]), std::move(data));
        printf("circular_buffer move constructed\n");
        ++_end;
    }

    template <typename T>
    template <typename... Args>
    inline void circular_buffer<T>::emplace_back(Args&&... args) {
        maybe_expand();
        std::construct_at(std::addressof(_impl[_end]), std::forward<Args>(args)...);
        ++_end;
    }

    template <typename T>
    inline T& circular_buffer<T>::access_element_unsafe(size_t idx) {
        return _impl[mask(_begin + idx)];
    }

    template <typename T>
    inline typename circular_buffer<T>::iterator circular_buffer<T>::erase(typename circular_buffer<T>::iterator first, typename circular_buffer<T>::iterator last) {
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
            auto new_end = std::move(last, end(), first);
            for (auto i = new_end, e = end(); i < e; ++i) {
                traits::destroy(_impl, this[i]);
            }

            _end = new_end.idx;
            return first;
        }
    }
};
