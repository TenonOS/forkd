#include "./include/bitmap.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int bit_number = sizeof(uint_bm) * 8;

static inline void clear_bit(int nr, volatile uint_bm *bitmap) {
    *bitmap &= ~(1 << (bit_number-1-nr));
}

static inline void set_bit(int nr, volatile uint_bm *bitmap) {
    *bitmap |= (1 << (bit_number-1-nr));
}

static inline int is_bit_set(int nr, volatile uint_bm bitmap) {
    return bitmap & (1 << (bit_number-1-nr));
}

static inline int find_free_bit(volatile uint_bm bitmap) {
    for (int i = bit_number - 1; i >= 0; --i) {
        if (!(bitmap & (1 << i))) {
            return bit_number - 1 - i;
        }
    }
    return -1;
}

static inline int find_rightmost_set_bit(volatile uint_bm bitmap) {
    if (bitmap == 0) {
        return -1;
    }
    int position = bit_number - 1;
    while (!(bitmap & 1)) {
        bitmap >>= 1;
        --position;
    }
    return position;
}

static inline int resize_bitmap(gpid_bitmap* gpbmap, int new_capacity) {
    if (new_capacity == gpbmap->capacity) {
        return 0;
    }
    uint_bm* new_bitmap = (uint_bm*)realloc(gpbmap->bitmap, sizeof(uint_bm)*(BYTE_NUMBER(new_capacity) + GID_OFFSET));
    if (new_bitmap == NULL) {
        printf("find_free_pid: realloc failed\n");
        // free(gpbmap->bitmap);
        return -1;
    }
    gpbmap->bitmap = new_bitmap;
    if (new_capacity > gpbmap->capacity) {
        memset(gpbmap->bitmap+GID_OFFSET+BYTE_NUMBER(gpbmap->capacity), 0, sizeof(uint_bm)*BYTE_NUMBER(gpbmap->capacity));
    } 
    gpbmap->capacity = new_capacity;
    return 0;
}

void init_gpid_bitmap(gpid_bitmap* gpbmap) {
    gpbmap->capacity = 2;
    gpbmap->max_gid = 0;
    gpbmap->bitmap = (uint_bm*)malloc(sizeof(uint_bm)*(BYTE_NUMBER(gpbmap->capacity) + GID_OFFSET));
    memset(gpbmap->bitmap, 0, sizeof(uint_bm)*(BYTE_NUMBER(gpbmap->capacity) + GID_OFFSET));
    // gid 0 is illegal
    set_bit(0, &gpbmap->bitmap[0]);
}

int find_free_gid(gpid_bitmap* gpbmap) {
    int free_gid = -1;
    for (int i = 0; i < GID_OFFSET; i++) {
        int temp_gid = find_free_bit(gpbmap->bitmap[i]);
        if (temp_gid == -1) {
            continue;
        } else {
            free_gid = temp_gid + i * bit_number;
            set_bit(temp_gid, &gpbmap->bitmap[i]);
            // if (free_gid < gpbmap->capacity) {
                // memset(&gpbmap->bitmap[free_gid * PID_BYTE_NUMBER + GID_OFFSET], 0, sizeof(uint_bm) * PID_BYTE_NUMBER);
            // }
            break;
        }
    }
    if (free_gid == -1) {
        printf("no free gid now\n");
    }
    if (gpbmap->max_gid ==-1 || free_gid > gpbmap->max_gid) {
        gpbmap->max_gid = free_gid;
    }
    return free_gid;
}

int find_free_pid(gpid_bitmap* gpbmap, int gid) {
    if (gid >= gpbmap->capacity) {
        int new_capacity = gpbmap->capacity;
        while (gid >= new_capacity) {
            new_capacity *= 2;
        }
        int result = resize_bitmap(gpbmap, new_capacity);
        if (result == -1) {
            printf("find_free_pid: resize_bitmap failed\n");
            return -1;
        }
    }
    int index = gid * PID_BYTE_NUMBER + GID_OFFSET;
    int free_pid = -1;
    for (int i = 0; i < PID_BYTE_NUMBER; ++i) {
        int temp_pid = find_free_bit(gpbmap->bitmap[index+i]);
        if (temp_pid == -1) {
            continue;
        } else {
            free_pid = temp_pid + i * bit_number;
            set_bit(temp_pid, &gpbmap->bitmap[index+i]);
            break;
        }
    }
    
    if (free_pid == -1) {
        printf("no free pid in group %d\n", gid);
    }
    return free_pid;
}

void release_gid(gpid_bitmap* gpbmap, int gid) {
    int index = gid / bit_number;
    if (index >= GID_OFFSET) {
        printf("release_gid: gid %d is out of range\n", gid);
        return;
    }
    int offset = gid % bit_number;
    clear_bit(offset, &gpbmap->bitmap[index]);
    memset(&gpbmap->bitmap[gid * PID_BYTE_NUMBER + GID_OFFSET], 0, sizeof(uint_bm) * PID_BYTE_NUMBER);

    if (gpbmap->max_gid == gid) {
        gpbmap->max_gid = -1;
        for (int i = GID_OFFSET - 1; i >= 0; --i) {
            int temp_gid = find_rightmost_set_bit(gpbmap->bitmap[i]);
            if (temp_gid != -1) {
                gpbmap->max_gid = temp_gid + i * bit_number;
                break;
            }
        }

        if (gpbmap->max_gid <= gpbmap->capacity / 4) {
            int result = resize_bitmap(gpbmap, gpbmap->capacity / 4);
            if (result == -1) {
                printf("release_gid: resize_bitmap failed\n");
            }
        }
    }
}

void release_pid(gpid_bitmap* gpbmap, int gid, int pid) {
    if (pid >= bit_number * PID_BYTE_NUMBER - 1) {
        printf("release_pid: pid %d is out of range\n", pid);
        return;
    }
    if (gid / bit_number >= GID_OFFSET) {
        printf("release_pid: gid %d is out of range\n", gid);
        return;
    }
    int index = gid * PID_BYTE_NUMBER + GID_OFFSET;
    int offset = pid % bit_number;
    clear_bit(offset, &gpbmap->bitmap[index + pid / bit_number]);

    // for (int i = 0; i < PID_BYTE_NUMBER; ++i) {
    //     if (gpbmap->bitmap[index + i] != 0) {
    //         return ;
    //     }
    // }

    // release_gid(gpbmap, gid);
}

void printBinary(uint_bm num) {
    uint_bm mask = 1 << (bit_number - 1);
    for (int i = 0; i < bit_number; i++) {
        if (num & mask) {
            printf("1");
        } else {
            printf("0");
        }
        mask >>= 1;
    }
    printf(" ");
}

void print_bitmap(gpid_bitmap* gpbmap) {
    for (int i = 0; i < GID_OFFSET + BYTE_NUMBER(gpbmap->capacity); ++i) {
        printBinary(gpbmap->bitmap[i]);
    }
    putchar('\n');
}

int is_gid_set(gpid_bitmap* gpbmap, int gid) {
    int index = gid / bit_number;
    if (index >= GID_OFFSET) {
        printf("is_gid_set: gid %d is out of range\n", gid);
        return 0;
    }
    int offset = gid % bit_number;
    return is_bit_set(offset, gpbmap->bitmap[index]);
}

int is_pid_set(gpid_bitmap* gpbmap, int gid, int pid) {
    if (pid >= bit_number * PID_BYTE_NUMBER - 1) {
        printf("is_pid_set: pid %d is out of range\n", pid);
        return 0;
    }
    if (gid / bit_number >= GID_OFFSET) {
        printf("is_pid_set: gid %d is out of range\n", gid);
        return 0;
    }
    int index = gid * PID_BYTE_NUMBER + GID_OFFSET + pid / bit_number;
    int offset = pid % bit_number;
    return is_bit_set(offset, gpbmap->bitmap[index]);
}