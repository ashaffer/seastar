#pragma once

namespace seastar {
	template<class T>
	concept NothrowMovable = std::is_nothrow_move_constructible_v<T>;

	template<class T>
	concept NothrowMoveAssignable = std::is_nothrow_move_assignable_v<T>;

	template<class T, class...Args>
	concept NothrowConstructible = std::is_nothrow_constructible_v<T, Args...>;

	template<class T>
	concept NothrowCopyable = std::is_nothrow_copy_constructible_v<T>;

	template<class T>
	concept NothrowCopyAssignable = std::is_nothrow_move_assignable_v<T>;

	template<class T>
	concept NothrowDestructible = std::is_destructible_v<T>;

	template<class T>
	concept NothrowErasable = NothrowMovable<T> && NothrowDestructible<T>;
};