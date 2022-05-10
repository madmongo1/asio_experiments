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

message(STATUS "BUILD_TESTING=${BUILD_TESTING}")
message(STATUS "ASIOEX_DEFAULT_ENABLE_TESTING=${ASIOEX_DEFAULT_ENABLE_TESTING}")
if (BUILD_TESTING)
    add_subdirectory(test)
endif ()
