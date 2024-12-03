#include "../include/error_handle.h"
#include "../include/server.h"
#include "../include/package.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>

#define ERROR_MESSAGE_SIZE 512

extern connect_item connect_list[];

void error_handling(enum ERROR_NUMBER error_number, int clnt_sock, void* data) {
    char* error_message = (char*)malloc(sizeof(char)*ERROR_MESSAGE_SIZE);
    switch (error_number)
    {
        case ERROR_NUMBER_IVALID_ARGS:
            sprintf(error_message, "error: -forkgroup is not found or parameter is invalid.");
            break;
        case ERROR_NUMBER_BITMAP_DIRTY:
            sprintf(error_message, "fatal error: pid_bitmap of gid %d is dirty.", (int)data);
            break;
        case ERROR_NUMBER_FORKARGS_NULL:
            sprintf(error_message, "error: args is NULL.");
            break;
        case ERROR_NUMBER_NOFREEGID:
            sprintf(error_message, "error: no gid available.");
            break;
        case ERROR_NUMBER_ILLEGAL_GIDPID:
            sprintf(error_message, "error: gid or pid is not available.");
            break;
        case ERROR_NUMBER_NOFREEPID:
            sprintf(error_message, "error: no pid available of group %d.", (int)data);
            break;
        default:
            free(error_message);
            return;
    }
    printf("%s\n", error_message);
    package* pack = connect_list[clnt_sock].send_package;
    pack->command = CMD_ERROR;
    pack->len = strlen(error_message);
    pack->data = (char*)malloc(pack->len);
    memcpy(pack->data, error_message, pack->len);
    free(error_message);
    return ;
}