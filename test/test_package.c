#include "../include/package.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main() {
    package p;
    p.command = CMD_FORKPARA;
    p.len = 3;
    p.data = "0 1";
    // memset(p.data, 'a', p.len);
    check_package(&p);
    int str_len;
    char* buf = serialize_package(&p, &str_len);
    write(fileno(stdout), buf, p.len+sizeof(p.command)+sizeof(p.len));
    package* p2 = deserialize_package(buf);
    check_package(p2);
}