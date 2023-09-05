#include <cstddef>
extern "C" {
    void __sanitizer_start_switch_fiber(void**, const void*, size_t);
    void __sanitizer_finish_switch_fiber(void*, const void**, size_t*);
}

#pragma message("sanitizers test")

int main() {
    __sanitizer_start_switch_fiber(nullptr, nullptr, 0);
    __sanitizer_finish_switch_fiber(nullptr, nullptr, nullptr);
    return 0;
}
