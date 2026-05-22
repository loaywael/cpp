cmake_minimum_required(VERSION 3.16)
project(PCD_Cache_Analysis)

# Enforce C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable Link-Time Optimization (LTO / IPO) for Release builds if supported
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
if(ipo_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    message(STATUS "Link-Time Optimization (LTO/IPO) enabled")
else()
    message(STATUS "LTO/IPO not supported: ${ipo_error}")
endif()

# Executable target
add_executable(${PROJECT_NAME} main.cpp utils.hpp)

# Enable native CPU instruction tuning for maximum vectorization (SIMD)
target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:-O3>
    $<$<CXX_COMPILER_ID:GNU>:-O3 -march=native>
    $<$<CXX_COMPILER_ID:Clang>:-O3 -march=native>
)

# Find and link Intel TBB for parallel algorithms
find_package(TBB REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE
    TBB::tbb
)