
add_executable(ipc-bus-client example_client.cpp)
target_include_directories(ipc-bus-client PRIVATE .)
target_link_libraries(ipc-bus-client PRIVATE ipc-bus)

add_executable(ipc-bus-server example_server.cpp)
target_include_directories(ipc-bus-server PRIVATE .)
target_link_libraries(ipc-bus-server PRIVATE ipc-bus)