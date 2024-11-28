#ifndef __ERROR_HANDLE_H__
#define __ERROR_HANDLE_H__

enum ERROR_NUMBER {
    NUMBER_SUCCESS = 0,
    ERROR_NUMBER_IVALID_ARGS,
    ERROR_NUMBER_BITMAP_DIRTY,
    ERROR_NUMBER_FORKARGS_NULL,
    ERROR_NUMBER_NOFREEGID,
    ERROR_NUMBER_ILLEGAL_GIDPID,
    ERROR_NUMBER_NOFREEPID,
};

void error_handling(enum ERROR_NUMBER error_number, int clnt_sock, void* data);

#endif