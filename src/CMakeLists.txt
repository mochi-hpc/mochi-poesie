# set source files
set (server-src-files
     Provider.cpp
     Backend.cpp
     javascript/JavascriptBackend.cpp
     jx9/Jx9Backend.cpp)

set (client-src-files
     Client.cpp
     MemoryView.cpp
     VmHandle.cpp)

add_library (jx9 STATIC jx9/jx9/jx9.c)
set_property (TARGET jx9 PROPERTY POSITION_INDEPENDENT_CODE ON)
target_compile_options (jx9 PRIVATE -Wno-stringop-overread)

add_library (duktape STATIC javascript/duktape/duktape.c)
set_property (TARGET duktape PROPERTY POSITION_INDEPENDENT_CODE ON)

if (ENABLE_LUA)
    list (APPEND server-src-files
          lua/LuaBackend.cpp
          lua/memory/lmemlib.c
          lua/memory/luamem.c)
    set (OPTIONAL_LUA ${SOL2_LIBRARIES} ${LUA_LIBRARIES})
endif ()

if (ENABLE_RUBY)
    list (APPEND server-src-files
          ruby/RubyBackend.cpp)
    set (OPTIONAL_RUBY ${MRUBY_LIBRARIES})
endif ()

if (ENABLE_PYTHON)
    list (APPEND server-src-files
          python/PythonBackend.cpp)
    set (OPTIONAL_PYTHON pybind11::embed)
endif ()

set (module-src-files
     BedrockModule.cpp)

# load package helper for generating cmake CONFIG packages
include (CMakePackageConfigHelpers)

# where to install files for "find_package"
set (poesie-pkg "share/cmake/poesie")

# set version fo shared library
set (poesie-vers "${POESIE_VERSION_MAJOR}.${POESIE_VERSION_MINOR}")

# server library
add_library (poesie-server ${server-src-files} ${dummy-src-files})
add_library (poesie::server ALIAS poesie-server)
target_compile_options (poesie-server PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries (poesie-server
    PUBLIC thallium nlohmann_json::nlohmann_json poesie-client
    PRIVATE spdlog::spdlog fmt::fmt jx9 duktape
    ${OPTIONAL_LUA} ${OPTIONAL_RUBY} ${OPTIONAL_PYTHON}
    coverage_config)
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
add_library (poesie::client ALIAS poesie-client)
target_compile_options (poesie-client PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries (poesie-client
    PUBLIC thallium nlohmann_json::nlohmann_json
    PRIVATE spdlog::spdlog fmt::fmt coverage_config)
target_include_directories (poesie-client PUBLIC $<INSTALL_INTERFACE:include>)
target_include_directories (poesie-client BEFORE PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>)
target_include_directories (poesie-client BEFORE PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)
set_target_properties (poesie-client
    PROPERTIES VERSION ${POESIE_VERSION}
    SOVERSION ${POESIE_VERSION_MAJOR})

if (${ENABLE_BEDROCK})
# bedrock module library
add_library (poesie-bedrock-module ${module-src-files})
target_compile_options (poesie-bedrock-module PRIVATE -Wall -Wextra -Wpedantic)
add_library (poesie::bedrock ALIAS poesie-bedrock-module)
target_link_libraries (poesie-bedrock-module poesie-server poesie-client bedrock::module-api coverage_config)
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
         DESTINATION include
         FILES_MATCHING PATTERN "*.hpp")
install (DIRECTORY ../include/poesie
         DESTINATION include
         FILES_MATCHING PATTERN "*.h")
install (FILES "${CMAKE_CURRENT_BINARY_DIR}/poesie-server.pc"
               "${CMAKE_CURRENT_BINARY_DIR}/poesie-client.pc"
         DESTINATION "lib/pkgconfig/")
