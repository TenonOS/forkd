rm -rf ./build
mkdir build
cd build
cmake ..
make
# ./test_package
# gdb ./server
./server_main 9190
# ./test_bitmap
