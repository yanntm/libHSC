# Find the GNU MP library and its C++ wrapper (gmp.h / gmpxx.h, libgmp /
# libgmpxx). GMP ships no CMake package, hence this module. Defines the
# imported targets GMP::gmp and GMP::gmpxx (the latter linking the former).
# Honours CMAKE_FIND_LIBRARY_SUFFIXES, so a static-first configuration picks
# the archives.

find_path(GMP_INCLUDE_DIR NAMES gmp.h)
find_path(GMPXX_INCLUDE_DIR NAMES gmpxx.h)
find_library(GMP_LIBRARY NAMES gmp)
find_library(GMPXX_LIBRARY NAMES gmpxx)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GMP
  REQUIRED_VARS GMP_LIBRARY GMPXX_LIBRARY GMP_INCLUDE_DIR GMPXX_INCLUDE_DIR)

if(GMP_FOUND AND NOT TARGET GMP::gmp)
  add_library(GMP::gmp UNKNOWN IMPORTED)
  set_target_properties(GMP::gmp PROPERTIES
    IMPORTED_LOCATION "${GMP_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${GMP_INCLUDE_DIR}")
  add_library(GMP::gmpxx UNKNOWN IMPORTED)
  set_target_properties(GMP::gmpxx PROPERTIES
    IMPORTED_LOCATION "${GMPXX_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${GMPXX_INCLUDE_DIR}"
    INTERFACE_LINK_LIBRARIES GMP::gmp)
endif()

mark_as_advanced(GMP_INCLUDE_DIR GMPXX_INCLUDE_DIR GMP_LIBRARY GMPXX_LIBRARY)
