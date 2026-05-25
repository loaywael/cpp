cmake_minimum_required(VERSION 3.15)
project(WorkStealingThreadPool CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable release-grade optimization flags
if(MSVC)
    add_compile_options(/O2 /W4)
else()
    add_compile_options(-O3 -Wall -Wextra -pthread)
endif()

add_executable(WorkStealing_Profiler main.cpp utils.hpp)

# Enforce native threading configuration
find_package(Threads REQUIRED)
target_link_libraries(WorkStealing_Profiler PRIVATE Threads::Threads)
