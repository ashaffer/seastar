#include <stdio.h>
#include <memory>
#include <seastar/core/circular_buffer.hh>
#include <seastar/core/reactor.hh>

namespace seastar {

    template <typename T, typename Alloc>
    void
    circular_buffer<T, Alloc>::expand(size_t new_cap) {
        if (new_cap > 8192) {
            printf("expanding %u-%lu: %lu begin, %lu end, %lu size, %lu new capacity\n", engine().cpu_id(), _impl.id, _impl.begin, _impl.end, size(), new_cap);
        }
        auto new_storage = _impl.allocate(new_cap);
        auto p = new_storage;
        try {
            for_each([this, &p] (T& obj) {
                transfer_pass1(_impl, &obj, p);
                p++;
            });
        } catch (...) {
            while (p != new_storage) {
                std::allocator_traits<Alloc>::destroy(_impl, --p);
            }
            _impl.deallocate(new_storage, new_cap);
            throw;
        }
        p = new_storage;
        for_each([this, &p] (T& obj) {
            transfer_pass2(_impl, &obj, p++);
        });
        std::swap(_impl.storage, new_storage);
        std::swap(_impl.capacity, new_cap);
        _impl.begin = 0;
        _impl.end = p - _impl.storage;
        _impl.deallocate(new_storage, new_cap);
    }
};