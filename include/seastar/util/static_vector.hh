#pragma once
#include <type_traits>
#include <array>
#include <memory>
#include <utility>
#include <algorithm>
#include "concepts.hh"

/*
 * Static vector implementation
 */

namespace seastar {

    template<class T, std::size_t Capacity>
    class static_vector {
    public:
        using value_type = T;
        using reference = value_type&;
        using lvalue_reference = value_type&&;
        using const_reference = const value_type&;
        using pointer = T*;
        using const_pointer = const pointer;
        using iterator = pointer;
        using const_iterator = const iterator;
        using size_type = std::size_t;
     
        constexpr static inline bool nothrow_erasable{NothrowErasable<value_type>};
        constexpr static inline bool nothrow_movable{NothrowMovable<value_type>};
        constexpr static inline bool nothrow_copyable{NothrowCopyable<value_type>};

    protected:
        using storage = std::aligned_storage_t<sizeof(value_type), alignof(value_type)>;
        std::array<storage, Capacity> _buf;
        // No stored data pointer (2026-09-06): the implicitly-defaulted copy/move used
        // to copy the raw pointer verbatim, so every copied/moved static_vector aliased
        // the SOURCE object's _buf -- saved_backtrace returned from current_backtrace()
        // iterated a dead stack frame (SIGSEGV at 0x11 inside operator<<: bot shard 3
        // 2026-08-19, ecmp_tool 2026-09-04 and 2026-09-06).
        size_type nth{0ul};
        inline constexpr pointer base () const noexcept {
            return reinterpret_cast<pointer>(const_cast<storage*>(_buf.data()));
        }
        // constexpr static inline bool nothrow_copyable{std::is_nothrow_copy_constructible_v<T>};
        // constexpr static inline bool nothrow_copy_assignable{std::is_nothrow_copy_assignable_v<T>};
        // constexpr static inline bool nothrow_movable{std::is_nothrow_move_constructible_v<T>};
        // constexpr static inline bool nothrow_move_assignable{std::is_nothrow_move_assignable_v<T>};
        // constexpr static inline bool nothrow_destructible{std::is_destructible_v<T>};
        // constexpr static inline bool nothrow_erasable{nothrow_movable && nothrow_destructible};
        // template<class...Args>
        // constexpr static inline bool nothrow_constructible{std::is_nothrow_constructible_v<T, Args...>};

        inline constexpr pointer ptr_to (size_type idx) noexcept {
            return std::addressof(base()[idx]);
        }
        inline constexpr void copy_to (size_type idx, const_reference val) noexcept(nothrow_copyable) { 
            new (ptr_to(idx)) T{val};
        }

        inline constexpr void move_to (size_type idx, lvalue_reference val) noexcept (nothrow_erasable) {
            new (ptr_to(idx)) T{std::move(val)};
        }

        template<class...Args>
        inline constexpr void emplace_to (size_type idx, Args...args) noexcept(NothrowConstructible<value_type, Args...>) {
            new (ptr_to(idx)) T{std::forward<Args>(args)...};
        }

    public:

        /**
         * Constructors
        **/

        constexpr static_vector () noexcept : nth{0} {

        }

        // Element-wise copy/move (see the note at _buf): trivially-copyable T keeps the
        // bytewise _buf copy of the old implicit members, minus the poison pointer.
        static_vector (const static_vector& o) noexcept(nothrow_copyable) : nth{0} {
            if constexpr (std::is_trivially_copyable_v<T>) { _buf = o._buf; nth = o.nth; }
            else { for (size_type i = 0; i < o.nth; ++i) { copy_to(nth, o.base()[i]); ++nth; } }
        }
        static_vector (static_vector&& o) noexcept(nothrow_movable) : nth{0} {
            if constexpr (std::is_trivially_copyable_v<T>) { _buf = o._buf; nth = o.nth; }
            else { for (size_type i = 0; i < o.nth; ++i) { move_to(nth, std::move(o.base()[i])); ++nth; } }
        }
        static_vector& operator= (const static_vector& o) noexcept(nothrow_copyable && nothrow_erasable) {
            if (this != &o) { clear(); for (size_type i = 0; i < o.nth; ++i) { copy_to(nth, o.base()[i]); ++nth; } }
            return *this;
        }
        static_vector& operator= (static_vector&& o) noexcept(nothrow_movable && nothrow_erasable) {
            if (this != &o) { clear(); for (size_type i = 0; i < o.nth; ++i) { move_to(nth, std::move(o.base()[i])); ++nth; } }
            return *this;
        }

        /**
         * Accessors
         */

        inline constexpr reference front () noexcept {
            return const_cast<reference>(cfront());
        }

        inline constexpr const_reference cfront () const noexcept {
            return *begin();
        }

        inline constexpr reference back () noexcept {
            return base()[std::max<size_t>(nth, 1) - 1];
        }

        inline constexpr const_reference cback () const noexcept {
            return base()[std::max<size_t>(nth, 1) - 1];
        }

        inline constexpr iterator begin () noexcept {
            return const_cast<iterator>(cbegin());
        }

        inline constexpr const_iterator cbegin () const noexcept {
            return std::addressof(base()[0]);
        }

        inline constexpr iterator end () noexcept {
            return const_cast<iterator>(cend());
        }

        inline constexpr const_iterator begin () const noexcept {
            return cbegin();
        }

        inline constexpr const_iterator end() const noexcept {
            return cend();
        }

        inline constexpr const_iterator cend () const noexcept {
            return std::addressof(base()[nth]);
        }

        inline constexpr pointer data () noexcept {
            return const_cast<pointer>((static_cast<const static_vector *>(this))->data());
        }

        inline constexpr const_pointer data() const noexcept {
            return base();
        }

        inline constexpr reference at (size_type idx) noexcept {
            return base()[idx];
        }

        inline constexpr const_reference at (size_type idx) const noexcept {
            return base()[idx];
        }

        inline constexpr bool operator== (const static_vector<T, Capacity>& other) const noexcept {
            if (other.size() != size()) {
                return false;
            }

            auto it{cbegin()};
            for (const auto& o : other) {
                if (o != *(it++)) {
                    return false;
                }
            }

            return true;
        }

        inline constexpr const_reference operator[] (size_type idx) const noexcept {
            return base()[idx];
        }

        inline constexpr reference operator[] (size_type idx) noexcept {
            return const_cast<reference>((static_cast<const static_vector *>(this))->operator[](idx));
        }

        /**
         * Getters
         */

        inline constexpr size_type capacity () const noexcept {
            return Capacity;
        }

        inline constexpr size_type size () const noexcept {
            return nth;
        }

        inline constexpr bool empty () const noexcept {
            return size() == 0;
        }

        inline constexpr bool full () const noexcept {
            return size() == capacity();
        }

        inline constexpr void next () noexcept {
            ++nth;
        }

        inline constexpr void resize (size_type n) noexcept {
            if (n <= capacity()) {
                while (n > nth) {
                    push_back({});
                }

                while (n < nth) {
                    pop_back();
                }
            }
        }

        /**
         * Modifiers
         */

        size_type erase (iterator first, iterator last)  noexcept (nothrow_erasable) {
            if (first >= last) {
                return 0ul;
            }

            size_type n{static_cast<size_type>(last - first)};
            
            if (nth < n) {
                return clear();
            }

            iterator cur{first};
            iterator last_occupied = end();
            iterator p{begin()};

            while (cur++ != last_occupied) {
                *(p++) = std::move(*cur);
            }

            while (cur++ != last) {
                (*(p++)).~T();
            }

            nth -= n;
            return n;
        }

        inline constexpr void push_back (const_reference val) noexcept (nothrow_copyable) {
            if (!full()) {
                // Make sure the copy constructor is called here
                copy_to(nth, val);
                ++nth;
            }
        }

        inline constexpr void push_back (lvalue_reference val) noexcept (nothrow_movable) {
            if (!full()) {
                // Use placement new with the move constructo
                move_to(nth, std::move(val));
                ++nth;
            }
        }

        template<class...Args>
        inline constexpr void emplace_back (Args...args) noexcept(NothrowConstructible<value_type, Args...>) {
            if (!full()) {
                emplace_to(nth, std::forward<Args>(args)...);
                ++nth;
            }
        }

        value_type pop_back () noexcept (nothrow_movable) {
            if (!empty()) {
                // This looks like a pessimizing move, but it isn't. We want the empty space to be in a moved
                // from state.
                return std::move(base()[--nth]);
            }

            return {};
       }

       size_type clear () noexcept (nothrow_erasable) {
            size_type n{size()};

            while (nth != 0) {
                std::destroy_at(std::addressof(base()[--nth]));
            }

            return n;
       }

        /**
         * Destructor
         */

       ~static_vector () noexcept (nothrow_erasable) {
            this->clear();
        }
    };

}; // end seastar