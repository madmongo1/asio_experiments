cmake_minimum_required(VERSION 3.20)

project(asio_experiments)

set(ASIOEX_DEFAULT_ENABLE_TESTING OFF)
if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    set(ASIOEX_DEFAULT_ENABLE_TESTING ON)
endif ()

include(CTest)
if (NOT BUILD_TESTING)
    if (ASIOEX_DEFAULT_ENABLE_TESTING)
        enable_testing()
    endif ()
endif ()

set(CMAKE_CXX_STANDARD 20)
message(STATUS "Compiler is ${CMAKE_CXX_COMPILER_ID}")
if (CMAKE_CXX_COMPILER_ID MATCHES "[Cc]lang")
    if (CMAKE_CXX_COMPILER_FRONTEND_VARIANT MATCHES "MSVC") # clang-cl
        add_compile_options("/await:strict")
    elseif (CMAKE_CXX_COMPILER_FRONTEND_VARIANT MATCHES "GNU") # clang native
    endif()
elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU") # both
        add_compile_options("-fconcepts-diagnostics-depth=10")
endif()

include(FetchContent)
FetchContent_Declare(asio
        GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
        GIT_TAG master
        GIT_SHALLOW Yes)

FetchContent_GetProperties(asio)
if (NOT asio_POPULATED)
    FetchContent_Populate(asio)
endif ()

find_package(Threads REQUIRED)

add_library(asio_asio INTERFACE)
target_include_directories(asio_asio INTERFACE "${asio_SOURCE_DIR}/asio/include")
target_include_directories(asio_asio INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
target_link_libraries(asio_asio INTERFACE Threads::Threads)
add_library(asio::asio ALIAS asio_asio)

find_package(Boost REQUIRED)

file(GLOB src_files CONFIGURE_DEPENDS "*.cpp")
foreach (src IN LISTS src_files)
    get_filename_component(target_name "${src}" NAME_WLE)
    add_executable("${target_name}" "${src}")
    target_link_libraries("${target_name}" PUBLIC asio::asio Boost::boost)
endforeach ()

message(STATUS "BUILD_TESTING=${BUILD_TESTING}")
message(STATUS "ASIOEX_DEFAULT_ENABLE_TESTING=${ASIOEX_DEFAULT_ENABLE_TESTING}")
if (BUILD_TESTING)
    #
    # test that all headers are self contained
    #

    file(GLOB_RECURSE all_headers RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/include" CONFIGURE_DEPENDS "include/*.hpp")
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
endif ()
