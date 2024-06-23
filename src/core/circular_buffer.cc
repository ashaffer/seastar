#include <stdio.h>
#include <memory>
#include <seastar/core/circular_buffer.hh>

namespace seastar {
    template <typename T>
    void circular_buffer<T>::expand() {
        reserve(std::max<size_t>(_capacity * 2, 1));
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
};