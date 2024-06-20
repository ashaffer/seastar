#pragma once
#include <iterator>

namespace seastar {

template<class T = std::size_t>
class counterator {
	class iterator {
		T cur;
	public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = T;
        using pointer           = T*;
        using reference         = T&;

		constexpr explicit iterator (T cur) noexcept : cur{cur} {} 
		iterator& operator++() { 
            ++cur; 
            return *this; 
        }
		iterator operator++(int i) { 
            iterator tmp{*this}; 
            cur += i;
            return tmp; 
        }

		iterator operator+= (T t) { iterator tmp{*this}; cur += t; return tmp; }
		bool operator==(iterator other) const { return other.cur == cur; }
		bool operator!=(iterator other) const { return other.cur != cur; }
		T operator* () const { return cur; }
	};

    T first, last;
public:
	constexpr explicit counterator(T first, T last) noexcept : first{first}, last{last} {}
	constexpr explicit counterator (T last) noexcept : first{0}, last{last} {}

	constexpr iterator begin () noexcept { return iterator(first); }
	constexpr iterator end () noexcept { return iterator(last); }
};

};