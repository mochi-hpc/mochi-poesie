# (C) 2022 The University of Chicago
# See COPYRIGHT in top-level directory.
cmake_minimum_required (VERSION 3.15)
project (poesie C CXX)
enable_testing ()

set (CMAKE_CXX_STANDARD 14)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
     "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wextra -Wall")
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wextra -Wall")

add_library (coverage_config INTERFACE)

option (ENABLE_COVERAGE   "Enable coverage reporting" OFF)
option (ENABLE_BEDROCK    "Build bedrock module" OFF)
option (ENABLE_LUA        "Build with Lua support" OFF)
option (ENABLE_PYTHON     "Build the Python support" OFF)
option (ENABLE_JAVASCRIPT "Build with QuickJS support" OFF)

if (ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options (coverage_config INTERFACE
      -O0        # no optimization
      -g         # generate debug info
      --coverage # sets all required flags
  )
  if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.13)
    target_link_options (coverage_config INTERFACE --coverage)
  else ()
    target_link_libraries (coverage_config INTERFACE --coverage)
  endif ()
endif ()

# add our cmake module directory to the path
set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
     "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# link shared lib with full rpath
set (CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
set (CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# setup cache variables for ccmake
if (NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE Release
         CACHE STRING "Choose the type of build." FORCE)
    set_property (CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
                  "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif ()
set (CMAKE_PREFIX_PATH "" CACHE STRING "External dependencies path")
set (BUILD_SHARED_LIBS "ON" CACHE BOOL "Build a shared library")

#find_package (nlohmann_json REQUIRED)
find_package (PkgConfig REQUIRED)

if (${ENABLE_BEDROCK})
    find_package (bedrock REQUIRED)
endif ()

# search for margo
pkg_check_modules (margo REQUIRED IMPORTED_TARGET margo)
pkg_check_modules (json-c REQUIRED IMPORTED_TARGET json-c)

if (ENABLE_LUA)
    find_package (Lua REQUIRED)
    #    find_package (sol2 REQUIRED)
    #    set (CMAKE_CXX_STANDARD 17)
    set (POESIE_HAS_LUA ON)
else ()
    set (POESIE_HAS_LUA OFF)
endif ()

if (ENABLE_PYTHON)
    find_package (Python3 COMPONENTS Interpreter Development REQUIRED)
    #find_package (pybind11 REQUIRED)
    set (POESIE_HAS_PYTHON ON)
else ()
    set (POESIE_HAS_PYTHON OFF)
endif ()

if (ENABLE_JAVASCRIPT)
    find_package (QuickJS REQUIRED)
    set (POESIE_HAS_JAVASCRIPT ON)
else ()
    set (POESIE_HAS_JAVASCRIPT OFF)
endif ()

# library version set here (e.g. for shared libs).
set (POESIE_VERSION_MAJOR 0)
set (POESIE_VERSION_MINOR 2)
set (POESIE_VERSION_PATCH 0)
set (POESIE_VERSION
  "${POESIE_VERSION_MAJOR}.${POESIE_VERSION_MINOR}.${POESIE_VERSION_PATCH}")

add_subdirectory (src)
add_subdirectory (test)
