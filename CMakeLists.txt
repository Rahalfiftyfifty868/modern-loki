cmake_minimum_required(VERSION 3.20)
project(modern-loki VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(modern_loki INTERFACE)
target_include_directories(modern_loki INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(modern_loki INTERFACE cxx_std_20)

if(MSVC)
    target_compile_options(modern_loki INTERFACE /W4 /permissive-)
else()
    target_compile_options(modern_loki INTERFACE -Wall -Wextra -Wpedantic)
endif()

option(LOKI_BUILD_TESTS "Build tests" ON)
option(LOKI_BUILD_BENCH "Build benchmarks" OFF)
option(LOKI_BUILD_MODULE "Build C++20 named module (requires CMake 3.28+)" OFF)

if(LOKI_BUILD_MODULE)
    if(CMAKE_VERSION VERSION_LESS "3.28")
        message(FATAL_ERROR "LOKI_BUILD_MODULE requires CMake 3.28+")
    endif()
    add_library(modern_loki_module)
    target_sources(modern_loki_module
        PUBLIC FILE_SET CXX_MODULES FILES
            include/loki/loki.cppm
    )
    target_include_directories(modern_loki_module PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/loki>
    )
    target_compile_features(modern_loki_module PUBLIC cxx_std_20)
    if(MSVC)
        target_compile_options(modern_loki_module PRIVATE /W4 /permissive-)
    else()
        target_compile_options(modern_loki_module PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

if(LOKI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(LOKI_BUILD_BENCH)
    add_subdirectory(bench)
endif()

include(CMakePackageConfigHelpers)
set(_install_targets modern_loki)
if(LOKI_BUILD_MODULE)
    list(APPEND _install_targets modern_loki_module)
endif()
install(TARGETS ${_install_targets} EXPORT modern_loki_targets)
install(DIRECTORY include/loki DESTINATION include)
install(EXPORT modern_loki_targets
    FILE modern-loki-config.cmake
    NAMESPACE loki::
    DESTINATION lib/cmake/modern-loki
)
