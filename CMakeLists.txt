cmake_minimum_required(VERSION 2.8)
project(server_client)
 
# Add the source in project root directory
aux_source_directory(. DIRSRCS)
# Add header file include directories
include_directories(./ ./include)
# Add block directories
# add_subdirectory(hello)
# add_subdirectory(world)
# Target
add_executable(server ./forkd.c)
add_executable(client ./client.c)
# target_link_libraries(helloworld hello world)