# forkd
An application helps to create new VMs.

architecture :

- util : 
    - args_handles : Convert the received string into qemu parameters.
    - bitmap.c : Using bitmap to manage GIDs and PIDs.
    - error_handle.c : Errors generated during parameter parsing process.
- server : 
    - server.c : Start the server, receive fork requests from qemu, parse startup parameters, and start a new virtual machine.
- client :
    - client.c : Send fork requests to the server. Just for test.
- include : Header files.
- main.c : Main function to start the server.
- run.sh : Build, compile, and run script files for forkd.
- CMakeLists.txt : CMake build file.

tip:
- The forkd server only supports the x86_64 architecture now.
- To initiate parameter checking, modify the CHECK_ARGS and CHECK_ARGS_CHILD in args_handle. c to 1