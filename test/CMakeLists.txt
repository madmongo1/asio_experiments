
enable_testing()

file(GLOB src_files CONFIGURE_DEPENDS "*.cpp")
foreach (src IN LISTS src_files)
    get_filename_component(target_name "${src}" NAME_WLE)
    add_executable("${target_name}" "${src}")
    target_link_libraries("${target_name}" PUBLIC asio::asio Boost::boost)
    add_test(NAME ${target_name} COMMAND $<TARGET_FILE:${target_name}>)
endforeach ()


#
# test that all headers are self contained
#

file(GLOB_RECURSE all_headers RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/../include" CONFIGURE_DEPENDS "include/*.hpp")
foreach (header_path IN LISTS all_headers)
    string(REGEX REPLACE "[/\\.]" "__" exename "${header_path}")
    message(STATUS "${header_path} -> ${exename}")
    configure_file(test/self_contained_test.cpp.in
            "asioex-test-self_contained-${exename}.cpp"
            @ONLY)
    add_executable("asioex-test-self_contained-${exename}"
            "asioex-test-self_contained-${exename}.cpp")
    target_include_directories("asioex-test-self_contained-${exename}" PRIVATE include)
    target_link_libraries("asioex-test-self_contained-${exename}" PRIVATE asio::asio)
    list(APPEND all_self_contained_tests "asioex-test-self_contained-${exename}")
    add_test(NAME "asioex-test-self_contained-${exename}" COMMAND "asioex-test-self_contained-${exename}")
endforeach ()

add_custom_target(asioex-test-all_self_contained
        DEPENDS ${all_self_contained_tests}
        COMMENT "Check all headers are self-contained")
