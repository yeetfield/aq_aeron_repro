set(CMAKE_VERBOSE_MAKEFILE ON)
cmake_minimum_required(VERSION 3.12)
project(aeron_public_demo)

add_executable(writer writer_main.cpp)
target_compile_options(writer PUBLIC -std=c++2a -O1 -Wall)
target_link_libraries(writer PUBLIC aeron aeron_client aeron_archive_client pthread)

add_executable(reader reader_main.cpp)
target_compile_options(reader PUBLIC -std=c++2a -Wall)
target_link_libraries(reader PUBLIC aeron aeron_client aeron_archive_client pthread)

