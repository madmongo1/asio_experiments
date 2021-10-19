cmake_minimum_required(VERSION 3.20)

project(asio_experiments)
set(CMAKE_CXX_STANDARD 20)
if (MSVC)
  add_compile_options("/await:strict")
endif()
include(FetchContent)
FetchContent_Declare(asio
        GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
        GIT_TAG master
        GIT_SHALLOW Yes)

FetchContent_GetProperties(asio)
if(NOT asio_POPULATED)
  FetchContent_Populate(asio)
endif()

find_package(Threads REQUIRED)

add_library(asio_asio INTERFACE)
target_include_directories(asio_asio INTERFACE "${asio_SOURCE_DIR}/asio/include")
target_include_directories(asio_asio INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
target_link_libraries(asio_asio INTERFACE Threads::Threads)
add_library(asio::asio ALIAS asio_asio)

file(GLOB src_files CONFIGURE_DEPENDS "*.cpp")
foreach(src IN LISTS src_files)
  get_filename_component(target_name "${src}" NAME_WLE)
  add_executable("${target_name}" "${src}")
  target_link_libraries("${target_name}" PUBLIC asio::asio)
endforeach()


#
# test that all headers are self contained
#

file(GLOB_RECURSE all_headers  RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/include" CONFIGURE_DEPENDS "include/*.hpp")
foreach(header_path IN LISTS all_headers)
  string(REGEX REPLACE "[/\\.]" "__" exename "${header_path}")
  message(STATUS "${header_path} -> ${exename}")
  configure_file(test/self_contained_test.cpp.in
          "asio-test-self_contained-${exename}.cpp"
          @ONLY)
  add_executable("asio-test-self_contained-${exename}"
          EXCLUDE_FROM_ALL
          "asio-test-self_contained-${exename}.cpp")
  target_include_directories("asio-test-self_contained-${exename}" PRIVATE include)
  target_link_libraries("asio-test-self_contained-${exename}" PRIVATE asio::asio)
  list(APPEND all_self_contained_tests "asio-test-self_contained-${exename}")
endforeach()

add_custom_target(asio-test-all_self_contained
        DEPENDS ${all_self_contained_tests}
        COMMENT "Check all headers are self-contained")
