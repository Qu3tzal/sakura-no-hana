cmake_minimum_required(VERSION 3.8)
project(snh)

set(CMAKE_CXX_STANDARD 14)

include_directories(
        C:/SFML-2.5.0/include)

link_directories(
        C:/SFML-2.5.0/bin/debug/lib C:/SFML-2.5.0/bin/release/lib)

file(GLOB_RECURSE SOURCE_FILES source/*.cpp source/*.hpp)

add_executable(snh ${SOURCE_FILES})
target_link_libraries(snh
        sfml-system
        sfml-window
        sfml-graphics
        sfml-audio
        sfml-network)
