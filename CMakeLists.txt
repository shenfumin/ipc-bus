cmake_minimum_required(VERSION 3.0)
project(ipc-bus)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -std=c++11 --whole-file")
find_package(msgpack-cxx REQUIRED)
find_package(cpp-ipc REQUIRED)
add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE include)
target_link_libraries(${PROJECT_NAME} INTERFACE pthread ipc msgpack-cxx rt)
add_subdirectory(example)