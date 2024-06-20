#pragma once 
#include "static_vector.hh"

namespace seastar {
	template<class T, std::size_t N>
	class ring_buffer : public static_vector<T, N> {
	public:
		using Base = static_vector<T, N>;
	    using value_type = Base::value_type;
	    using reference = Base::reference;
	    using const_reference = Base::const_reference;
	    using pointer = Base::pointer;
	    using const_pointer = Base::const_pointer;
	    using iterator = Base::iterator;
	    using const_iterator = Base::const_iterator;
	    using size_type = Base::size_type;

	 	constexpr static inline bool nothrow_erasable{NothrowErasable<value_type>};
    	constexpr static inline bool nothrow_movable{NothrowMovable<value_type>};
    	constexpr static inline bool nothrow_copyable{NothrowCopyable<value_type>};

	private:
		size_type read_idx{0};
		size_type write_idx{0};

	public:
		size_type erase (iterator from, iterator to) {
			size_type count{0};

			if (from > to) {
				count += Base::erase(to, Base::end());
				from = Base::begin();
			}

			count += Base::erase(from, to);
			return count;
		}

		constexpr inline iterator bump () noexcept {
			auto p{end()};
			write_idx = ++write_idx % N;
			if (read_idx == write_idx) {
				read_idx = ++read_idx % N;
			}

			return p;
		}

		inline std::size_t consume_all (auto&& fn) {
			std::size_t i{0};

			while (consume(std::forward<decltype(fn)>(fn))) {
				++i;
			}

			return i;
		}

		bool consume (auto&& fn) {
			if (write_idx != read_idx) {
				auto tmp{Base::at(read_idx++ % N)};
				fn(std::move(tmp));
				return true;
			}

			return false;
		}

		inline void push (auto&& val) {
			push_back(std::forward<decltype(val)>(val));
		}

		const_iterator begin () const noexcept {
			return Base::begin() + read_idx;
		}

		const_iterator end () const noexcept {
			return Base::end() + write_idx;
		}

		iterator begin () noexcept {
			return Base::begin() + read_idx;
		}

		iterator end () noexcept {
			return Base::end() + write_idx;
		}

		// void pop_back () {
		// 	return 
		// }

		void push_back (T&& t) noexcept (nothrow_movable) {
			iterator p = bump();
			new (p) value_type{std::move(t)};
		}
		
		void push_back (const T& t) noexcept (nothrow_copyable) {
			iterator p{bump()};
			new (p) value_type{t};
		}
	
		template<class...Args>
		void emplace_back (Args...args) {
			new (bump()) T{std::forward<Args>(args)...};
		}
	};
};

// namespace std::{
// 	template<class T>
// 	struct StackAllocator : Alllocator<T> {
// 		StackAllocator
// 		inline constexpr allocate (std::size_t) {
			
// 		}
// 		inline constexpr pointer ptr_to (std::size_t idx) noexcept {
// 		    return std::addressof(_data[idx]);
// 		}
// 		inline constexpr void allocate (std::size_t idx, const T& val) noexcept(NothrowCopyable<T>) { 
// 		    new (ptr_to(idx)) T{val};
// 		}

// 		inline constexpr void move_to (std::size_t idx, T&& val) noexcept (NothrowMovable<T>) {
// 		    new (ptr_to(idx)) T{std::move(val)};
// 		}

// 		template<class...Args>
// 		inline constexpr void emplace_to (std::size_t idx, Args...args) noexcept(NothrowConstructible<T, Args...>) {
// 		    new (ptr_to(idx)) T{std::forward<Args>(args)...};
// 		}
// 	}