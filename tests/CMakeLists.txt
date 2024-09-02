file (GLOB test-sources ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
foreach (test-source ${test-sources})
    get_filename_component (test-target ${test-source} NAME_WE)
    add_executable (${test-target} ${test-source})
    target_link_libraries (${test-target} PRIVATE
        Catch2::Catch2WithMain poesie::server poesie::client fmt::fmt)
    add_test (NAME ${test-target} COMMAND timeout 60s ./${test-target})
endforeach ()

file (GLOB example-files ${CMAKE_CURRENT_SOURCE_DIR}/example*)
foreach (example ${example-files})
    get_filename_component (example-name ${example} NAME)
    configure_file (${example} ${CMAKE_CURRENT_BINARY_DIR}/${example-name} COPYONLY)
endforeach ()
