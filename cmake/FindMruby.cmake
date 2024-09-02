# FindMruby.cmake
#
# This module finds the mruby library and its header files.
# It defines the following variables:
#
# MRUBY_FOUND        - True if mruby was found.
# MRUBY_INCLUDE_DIRS - The directory containing mruby.h.
# MRUBY_LIBRARIES    - The mruby library file to link against.
# MRUBY_VERSION      - The version of mruby found.

find_path (MRUBY_INCLUDE_DIR
    NAMES mruby.h
    PATHS
        ENV MRUBY_INCLUDE_DIR
        /usr/local/include
        /usr/include
        /opt/local/include
        /opt/include
        /usr/local/mruby/include
        /usr/local/mruby-*/include
)

find_library (MRUBY_LIBRARY
    NAMES mruby
    PATHS
        ENV MRUBY_LIBRARY
        /usr/local/lib
        /usr/lib
        /opt/local/lib
        /opt/lib
        /usr/local/mruby/lib
        /usr/local/mruby-*/lib
)

# Check if the library and header were found
if (MRUBY_INCLUDE_DIR AND MRUBY_LIBRARY)
    set (MRUBY_FOUND TRUE)

    # Set the include dirs and libraries variables
    set (MRUBY_INCLUDE_DIRS ${MRUBY_INCLUDE_DIR})
    set (MRUBY_LIBRARIES ${MRUBY_LIBRARY})

    # Try to determine the version
    if (EXISTS "${MRUBY_INCLUDE_DIR}/mruby/version.h")
        file (READ "${MRUBY_INCLUDE_DIR}/mruby/version.h" MRUBY_VERSION_HEADER)
        string (REGEX MATCH "#define[ \t]+MRUBY_RUBY_VERSION[ \t]+\"([^\"]+)\"" _ ${MRUBY_VERSION_HEADER})
        set (MRUBY_VERSION ${CMAKE_MATCH_1})
    else ()
        set (MRUBY_VERSION "unknown")
    endif ()

    # Provide a summary message
    message (STATUS "Found mruby: ${MRUBY_LIBRARY} (found version \"${MRUBY_VERSION}\")")
else ()
    set (MRUBY_FOUND FALSE)
    message (STATUS "Could NOT find mruby")
endif ()

mark_as_advanced (MRUBY_INCLUDE_DIR MRUBY_LIBRARY)

