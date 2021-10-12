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
target_link_libraries(asio_asio INTERFACE Threads::Threads)
add_library(asio::asio ALIAS asio_asio)

file(GLOB src_files CONFIGURE_DEPENDS "*.cpp")
foreach(src IN LISTS src_files)
  get_filename_component(target_name "${src}" NAME_WLE)
  add_executable("${target_name}" "${src}")
  target_link_libraries("${target_name}" PUBLIC asio::asio)
endforeach()
