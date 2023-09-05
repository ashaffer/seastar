#if __cplusplus >= 201703L && __has_include(<filesystem>)
#include <filesystem>
namespace filesystem = std::filesystem;
#else
#include <experimental/filesystem>
namespace filesystem = std::experimental::filesystem;
#endif

#pragma message("std filesystem test")

int main() {
    filesystem::path path("/root");
    (void)path;
}
