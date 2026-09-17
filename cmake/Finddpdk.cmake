#
# Finddpdk.cmake -- triarb fork, DPDK >= 20.11 (2026-09-17, binancebot5 / Nitro v6 port).
#
# DPDK dropped its per-PMD static library names and the make build in 20.11; the supported discovery is pkg-config
# (libdpdk.pc, installed by `ninja install`). This module exposes:
#   dpdk_FOUND, dpdk_INCLUDE_DIRS, dpdk_CFLAGS_OTHER (e.g. -include rte_config.h -mrtm),
#   dpdk_STATIC_LDFLAGS (the full static link line incl. -Wl,--whole-archive <PMDs> -Wl,--no-whole-archive),
#   dpdk::dpdk  -- INTERFACE target carrying the include dirs and compile options ONLY (the PMD objects are folded
#                  into seastar-dpdk.o by the whole-archive partial link in CMakeLists.txt, as before).
#

find_package (PkgConfig REQUIRED)
pkg_check_modules (dpdk QUIET libdpdk)

include (FindPackageHandleStandardArgs)

find_package_handle_standard_args (dpdk
  REQUIRED_VARS
    dpdk_FOUND
    dpdk_INCLUDE_DIRS
  VERSION_VAR dpdk_VERSION)

if (dpdk_FOUND AND NOT (TARGET dpdk::dpdk))
  add_library (dpdk::dpdk INTERFACE IMPORTED)

  set_target_properties (dpdk::dpdk
    PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${dpdk_INCLUDE_DIRS}"
      INTERFACE_COMPILE_OPTIONS "${dpdk_CFLAGS_OTHER}")

  # Everything the static link needs, in pkg-config's order; consumed by the seastar-dpdk.o partial link.
  string (REPLACE ";" " " dpdk_STATIC_LDFLAGS_STR "${dpdk_STATIC_LDFLAGS}")
  set (dpdk_LIBRARIES ${dpdk_STATIC_LDFLAGS})
  # The system libraries DPDK's static archives depend on (-lnuma -lm -ldl -lpthread, and whatever optional deps meson found:
  # archive/pcap/bsd/elf/...) are NOT folded into seastar-dpdk.o (see CMakeLists.txt); consumers must link them, so carry
  # them on the interface target.
  set (dpdk_SYSTEM_LIBRARIES "")
  foreach (arg IN LISTS dpdk_STATIC_LDFLAGS)
    if (arg MATCHES "^-l[^:]" AND NOT arg MATCHES "^-lrte_")
      list (APPEND dpdk_SYSTEM_LIBRARIES ${arg})
    endif ()
  endforeach ()
  list (REMOVE_DUPLICATES dpdk_SYSTEM_LIBRARIES)
  set_property (TARGET dpdk::dpdk PROPERTY INTERFACE_LINK_LIBRARIES "${dpdk_SYSTEM_LIBRARIES}")
  message (STATUS "dpdk system libraries for consumers: ${dpdk_SYSTEM_LIBRARIES}")
  message (STATUS "dpdk ${dpdk_VERSION} via pkg-config: ${dpdk_INCLUDE_DIRS}")
endif ()
