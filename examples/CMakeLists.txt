add_executable (example-server ${CMAKE_CURRENT_SOURCE_DIR}/server.cpp)
target_link_libraries (example-server fmt::fmt spdlog::spdlog poesie-server)

add_executable (example-client ${CMAKE_CURRENT_SOURCE_DIR}/client.cpp)
target_link_libraries (example-client fmt::fmt spdlog::spdlog poesie-client)
