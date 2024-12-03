#ifndef __HANDLE_ARGS__
#define __HANDLE_ARGS__

#include "error_handle.h"

void check_args(char* command, char** args);
void free_args(char* command, char** args);
void get_fork_args(int clnt_sock);

#endif