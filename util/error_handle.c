#include "../include/error_handle.h"
#include "../include/server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>

#define ERROR_MESSAGE_SIZE 512

extern connect_item connect_list[];

void error_handling(enum ERROR_NUMBER error_number, int clnt_sock, void* data) {
    char* error_message = NULL;
    int malloc_flag = 0;
    switch (error_number)
    {
    case ERROR_NUMBER_IVALID_ARGS:
        error_message = "error: -forkgroup is not found or parameter is invalid.";
        break;
    case ERROR_NUMBER_BITMAP_DIRTY:
        error_message = (char*)malloc(sizeof(char)*ERROR_MESSAGE_SIZE);
        sprintf(error_message, "fatal error: pid_bitmap of gid %d is dirty.", (int)data);
        malloc_flag = 1;
        break;
    case ERROR_NUMBER_FORKARGS_NULL:
        error_message = "error: args is NULL.";
        break;
    case ERROR_NUMBER_NOFREEGID:
        error_message = "error: no gid available.";
        break;
    case ERROR_NUMBER_ILLEGAL_GIDPID:
        error_message = "error: gid or pid is not available.";
        break;
    case ERROR_NUMBER_NOFREEPID:
        error_message = (char*)malloc(sizeof(char)*ERROR_MESSAGE_SIZE);
        sprintf(error_message, "error: no pid available of group %d.", (int)data);
        malloc_flag = 1;
        break;
    
    default:
        break;
    }
    printf("%s\n", error_message);
    memcpy(connect_list[clnt_sock].wbuffer, error_message, strlen(error_message));
    connect_list[clnt_sock].windex = strlen(error_message);
    set_event(clnt_sock, EPOLLOUT|EPOLLET, 0);
    if (malloc_flag) {
        free(error_message);
    }
    return ;
}