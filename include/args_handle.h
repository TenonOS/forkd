#ifndef __HANDLE_ARGS__
#define __HANDLE_ARGS__

#include "error_handle.h"


char** split_args(char* args, char** command);
void check_args(char* command, char** args);
void free_args(char* command, char** args);
enum ERROR_NUMBER get_forkgroup_parameters(char** args, int* receive_gid, int* receive_pid, int* fork_group_parameter_index);
void get_fork_args(int clnt_sock);

#endif