#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "Seastar::seastar" for configuration "Release"
set_property(TARGET Seastar::seastar APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(Seastar::seastar PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libseastar.a"
  )

list(APPEND _cmake_import_check_targets Seastar::seastar )
list(APPEND _cmake_import_check_files_for_Seastar::seastar "${_IMPORT_PREFIX}/lib/libseastar.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
