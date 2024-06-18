// #include <seastar/util/std-compat.hh>
#include <filesystem>
#include <seastar/core/sstring.hh>

namespace seastar {

sstring read_first_line(std::filesystem::path sys_file);

}
