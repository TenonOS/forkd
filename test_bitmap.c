#include "./include/bitmap.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int main() {
    gpid_bitmap gpbmap;
    init_gpid_bitmap(&gpbmap);
    int gid = find_free_gid(&gpbmap);
    printf("gid: %d\n", gid);
    assert(gid == 0);

    for (int i = 0; i < 10; ++i) {
        int gid = find_free_gid(&gpbmap);
        // printf("gid: %d\n", gid);
    }
    print_bitmap(&gpbmap);

    assert(gpbmap.max_gid == 10);
    assert(gpbmap.capacity == 1);

    for (int i = 0; i < 16; ++i) {
        int pid = find_free_pid(&gpbmap, 5);
        // printf("pid: %d\n", pid);
    }
    print_bitmap(&gpbmap);

    for (int i = 0; i < 16; ++i) {
        release_pid(&gpbmap, 5, i);
    }

    print_bitmap(&gpbmap);

    assert(gpbmap.capacity == 8);

    for (int i = 3; i <= 10; ++i) {
        release_gid(&gpbmap, i);
    }
    print_bitmap(&gpbmap);

    assert(gpbmap.max_gid == 2);
    assert(gpbmap.capacity == 2);

    printf("all test passed\n");

}