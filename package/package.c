#include "../include/package.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char* serialize_package(package *p, int* str_len) {
    *str_len = sizeof(p->command) + sizeof(p->len) + p->len;
    char* buf = (char*)malloc(*str_len);
    int offset = 0;
    memcpy(buf, &p->command, sizeof(p->command));
    offset += sizeof(p->command);
    memcpy(buf + offset, &p->len, sizeof(p->len));
    if (p->len > 0) {
        offset += sizeof(p->len);
        memcpy(buf + offset, p->data, p->len);
    }
    return buf;
}

package* deserialize_package(char *buf) {
    package *p = (package*)malloc(sizeof(package));
    int offset = 0;
    memcpy(&p->command, buf, sizeof(p->command));
    offset += sizeof(p->command);
    memcpy(&p->len, buf + offset, sizeof(p->len));
    if (p->len > 0) {
        offset += sizeof(p->len);
        p->data = (char*)malloc(p->len);
        memcpy(p->data, buf + offset, p->len);
    }
    return p;
}

void check_package(package *p) {
    printf("-----------------------------------------\n");
    printf("check package:\n");
    printf("%d %d ", p->command, p->len);
    fflush(stdout);
    write(fileno(stdout), p->data, p->len);
    printf("\n-----------------------------------------\n");
}

