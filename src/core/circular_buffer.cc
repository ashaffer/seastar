#include <stdio.h>
#include <memory>
#include <seastar/core/circular_buffer.hh>

namespace seastar {
    template <typename T>
    inline size_t circular_buffer<T>::mask(size_t idx) const {
        return idx & (_capacity - 1);
    }

    template <typename T>
    inline bool circular_buffer<T>::empty() const {
        return _begin == _end;
    }

    template <typename T>
    inline size_t circular_buffer<T>::size() const {
        return _end - _begin;
    }

    template <typename T>
    inline size_t circular_buffer<T>::capacity() const {
        return _capacity;
    }

    template <typename T>
    inline void circular_buffer<T>::reserve(size_t size) {
        if (capacity() < size) {
            // Make sure that the new capacity is a power of two.
            realloc(size_t(1) << log2ceil(size));
        }
    }

    template <typename T>
    inline void circular_buffer<T>::clear() {
        erase(begin(), end());
    }

    template <typename T>
    inline circular_buffer<T>::circular_buffer(circular_buffer&& x) noexcept
        : _impl(std::move(x._impl)), _begin{x._begin}, _end{x._end}, _capacity{x._capacity} {
        x._impl = nullptr;
        x._begin = 0;
        x._end = 0;
        x._capacity = 0;
    }

    template <typename T>
    inline T& circular_buffer<T>::front() {
        return _impl[mask(_begin)];
    }

    template <typename T>
    inline const T& circular_buffer<T>::front() const {
        return _impl[mask(_begin)];
    }

    template <typename T>
    inline T& circular_buffer<T>::back() {
        return _impl[mask(_end - 1)];
    }

    template <typename T>
    inline const T& circular_buffer<T>::back() const {
        return _impl[mask(_end - 1)];
    }

    template <typename T>
    inline void circular_buffer<T>::pop_front() {
        std::destroy_at(std::addressof(front()));
        ++_begin;
    }

    template <typename T>
    inline void circular_buffer<T>::pop_back() {
        std::destroy_at(std::addressof(back()));
        --_end;
    }

    template <typename T>
    inline T& circular_buffer<T>::operator[](size_t idx) {
        return _impl[mask(_begin + idx)];
    }

    template <typename T>
    inline circular_buffer<T>& circular_buffer<T>::operator=(circular_buffer&& x) noexcept {
        if (this != &x) {
            this->~circular_buffer();
            new (this) circular_buffer(std::move(x));
        }
        return *this;
    }

    template <typename T>
    template <typename Func>
    inline void circular_buffer<T>::for_each(Func&& func) {
        for (auto&& p{begin()}, e{end()}; p != e; ++p) {
            func(*p);
        }
    }

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
    void circular_buffer<T>::expand() {
        reserve(std::max<size_t>(_capacity * 2, 1));
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
    void circular_buffer<T>::realloc(size_t new_cap) {
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