cmake_minimum_required(VERSION 2.8)
project(server_client)

set(CMAKE_BUILD_TYPE debug CACHE STRING "Build Type" FORCE)
 
# Add the source in project root directory
# aux_source_directory(. DIRSRCS)
# Add header file include directories
include_directories(./ ./include)
# Add block directories
add_subdirectory(util)
add_subdirectory(package)
add_subdirectory(server)
# add_dependencies(server util)
add_subdirectory(client)
# Target
add_executable(server_main ./main.c)
# server 依赖于 util，注意库的链接顺序
target_link_libraries(server_main server package util)

add_executable(client_main ./client/client.c)
target_link_libraries(client_main util client)

# add_executable(test_bitmap ./test/test_bitmap.c)
# target_link_libraries(test_bitmap util)

# add_executable(test_package ./test/test_package.c)
# target_link_libraries(test_package package)