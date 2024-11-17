rm -rf ./build
mkdir build
cd build
cmake ..
make
./server 9190
# ./test_bitmap
