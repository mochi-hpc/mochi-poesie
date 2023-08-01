# Find the native QuickJS headers and libraries.
#
# QuickJS_INCLUDE_DIRS     - where to find quickjs.h.
# QuickJS_LIBRARIES        - List of libraries when using mapm.
# QuickJS_FOUND            - True if mapm found.

# Look for the header file.
FIND_PATH (QuickJS_INCLUDE_DIR NAMES quickjs/quickjs.h)

# Look for the library.
FIND_LIBRARY (QuickJS_LIBRARY NAMES quickjs)

# Handle the QUIETLY and REQUIRED arguments and set QuickJS_FOUND to TRUE if all listed variables are TRUE.
INCLUDE (FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS (QuickJS DEFAULT_MSG QuickJS_LIBRARY QuickJS_INCLUDE_DIR)

# Copy the results to the output variables.
IF (QuickJS_FOUND)
    SET (QuickJS_LIBRARIES ${QuickJS_LIBRARY})
    SET (QuickJS_INCLUDE_DIRS ${QuickJS_INCLUDE_DIR})
ELSE (QuickJS_FOUND)
    SET (QuickJS_LIBRARIES)
    SET (QuickJS_INCLUDE_DIRS)
ENDIF (QuickJS_FOUND)

MARK_AS_ADVANCED (QuickJS_INCLUDE_DIRS QuickJS_LIBRARIES)
