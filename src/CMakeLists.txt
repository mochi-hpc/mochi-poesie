# set source files
set (server-src-files
     poesie-server.c
     poesie-vm-impl.c)

set (client-src-files
     poesie-client.c)

set (LANG_DEPENDENCIES "")

if (ENABLE_LUA)
    list (APPEND server-src-files
          lang/poesie-lua.c)
    list (APPEND DEPENDENCIES ${LUA_LIBRARIES})
    include_directories (${LUA_INCLUDE_DIR})
endif (ENABLE_LUA)

if (ENABLE_PYTHON)
     list (APPEND server-src-files
           lang/poesie-python.c)
   list (APPEND LANG_DEPENDENCIES Python3::Python) #pybind11::embed)
endif (ENABLE_PYTHON)

# load package helper for generating cmake CONFIG packages
include (CMakePackageConfigHelpers)

# where to install files for "find_package"
set (poesie-pkg "share/cmake/poesie")

# set version fo shared library
set (poesie-vers "${POESIE_VERSION_MAJOR}.${POESIE_VERSION_MINOR}")

# server library
add_library (poesie-server ${server-src-files} ${backend-src-files})
target_link_libraries (poesie-server
    PUBLIC PkgConfig::margo
    PRIVATE coverage_config ${LANG_DEPENDENCIES})
target_compile_definitions (poesie-server PRIVATE -DJSON_HAS_CPP_14)
target_include_directories (poesie-server PUBLIC $<INSTALL_INTERFACE:include>)
target_include_directories (poesie-server BEFORE PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>)
target_include_directories (poesie-server BEFORE PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)
set_target_properties (poesie-server
    PROPERTIES VERSION ${POESIE_VERSION}
    SOVERSION ${POESIE_VERSION_MAJOR})

# client library
add_library (poesie-client ${client-src-files})
target_link_libraries (poesie-client
    PUBLIC PkgConfig::margo
    PRIVATE coverage_config)
target_compile_definitions (poesie-client PRIVATE -DJSON_HAS_CPP_14)
target_include_directories (poesie-client PUBLIC $<INSTALL_INTERFACE:include>)
target_include_directories (poesie-client BEFORE PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>)
target_include_directories (poesie-client BEFORE PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)
set_target_properties (poesie-client
    PROPERTIES VERSION ${POESIE_VERSION}
    SOVERSION ${POESIE_VERSION_MAJOR})

if (${ENABLE_BEDROCK})
  # bedrock-module library
  add_library (poesie-bedrock-module ${bedrock-module-src-files})
  target_link_libraries (poesie-bedrock-module
    PUBLIC poesie-server
    PUBLIC poesie-client
    PUBLIC bedrock-server
    PRIVATE coverage_config)
  target_compile_definitions (poesie-bedrock-module PRIVATE -DJSON_HAS_CPP_14)
  target_include_directories (poesie-bedrock-module PUBLIC $<INSTALL_INTERFACE:include>)
  target_include_directories (poesie-bedrock-module BEFORE PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>)
  target_include_directories (poesie-bedrock-module BEFORE PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)
  set_target_properties (poesie-bedrock-module
    PROPERTIES VERSION ${POESIE_VERSION}
    SOVERSION ${POESIE_VERSION_MAJOR})
endif ()

# installation stuff (packaging and install commands)
write_basic_package_version_file (
    "poesie-config-version.cmake"
    VERSION ${POESIE_VERSION}
    COMPATIBILITY AnyNewerVersion)

# generate our config file for find_package()
set (INCLUDE_INSTALL_DIR include/)
set (LIB_INSTALL_DIR lib/)
configure_package_config_file (poesie-config.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/poesie-config.cmake
  INSTALL_DESTINATION ${LIB_INSTALL_DIR}/poesie/cmake
  PATH_VARS INCLUDE_INSTALL_DIR)

# some bits for the pkg-config file
set (DEST_DIR "${CMAKE_INSTALL_PREFIX}")
set (SERVER_PRIVATE_LIBS "-lpoesie-server")
set (CLIENT_PRIVATE_LIBS "-lpoesie-client")
configure_file ("poesie-server.pc.in" "poesie-server.pc" @ONLY)
configure_file ("poesie-client.pc.in" "poesie-client.pc" @ONLY)

# configure config.h
configure_file ("config.h.in" "config.h" @ONLY)

# "make install" rules
install (TARGETS poesie-server poesie-client
         EXPORT poesie-targets
         ARCHIVE DESTINATION lib
         LIBRARY DESTINATION lib)
if (${ENABLE_BEDROCK})
    install (TARGETS poesie-bedrock-module
             ARCHIVE DESTINATION lib
             LIBRARY DESTINATION lib)
endif ()
install (EXPORT poesie-targets
         DESTINATION ${poesie-pkg}
         FILE "poesie-targets.cmake")
install (FILES "${CMAKE_CURRENT_BINARY_DIR}/poesie-config.cmake"
               "${CMAKE_CURRENT_BINARY_DIR}/poesie-config-version.cmake"
         DESTINATION ${poesie-pkg} )
install (DIRECTORY ../include/poesie
         DESTINATION include)
install (FILES "${CMAKE_CURRENT_BINARY_DIR}/poesie-server.pc"
               "${CMAKE_CURRENT_BINARY_DIR}/poesie-client.pc"
         DESTINATION "lib/pkgconfig/")
