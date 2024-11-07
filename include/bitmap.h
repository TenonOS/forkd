#ifndef __BITMAP_H__
#define __BITMAP_H__

#include <stdint.h>

#define GID_OFFSET 2
#define PID_BYTE_NUMBER 3
#define BYTE_NUMBER(capacity) (capacity * PID_BYTE_NUMBER)

typedef uint8_t uint_bm;

typedef struct {
    uint_bm* bitmap;
    int capacity;
    int max_gid;
} gpid_bitmap;

void init_gpid_bitmap(gpid_bitmap* gpbmap);
int find_free_gid(gpid_bitmap* gpbmap);
int find_free_pid(gpid_bitmap* gpbmap, int gid);
void release_gid(gpid_bitmap* gpbmap, int gid);
void release_pid(gpid_bitmap* gpbmap, int gid, int pid);
void print_bitmap(gpid_bitmap* gpbmap);
int is_gid_set(gpid_bitmap* gpbmap, int gid);
int is_pid_set(gpid_bitmap* gpbmap, int gid, int pid);

// 增加两个 check 函数

#endif