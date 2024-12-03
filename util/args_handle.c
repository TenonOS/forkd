#include "../include/args_handle.h"
#include "../include/bitmap.h"
#include "../include/server.h"
#include "../include/error_handle.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <errno.h>

#define PARAMETER_SIZE 128
#define ARG_NUMBER 128
#define BUF_SIZE 1024
#define CHECK_ARGS 0
#define CHECK_ARGS_CHILD 0

extern int epfd;
extern gpid_bitmap gpbmap;
extern connect_item connect_list[];

static inline int check_number(char* str);
static int endsWith(const char*str, const char* suffix, const int suffixLen);
static enum ERROR_NUMBER get_forkgroup_parameters(char** args, int* receive_gid, int* receive_pid, int* fork_group_parameter_index);
static char** split_args(char* args, char** command);

void check_args(char* command, char** args) {
    printf("check args\n");
    printf("command: %s\n", command);
    int i = 0;
    while (args[i] != NULL) {
        printf("args[%d]: %s\n", i, args[i]);
        ++i;
    }
    printf("check args end\n");
}

void free_args(char* command, char** args) {
    if (command != NULL) {
        free(command);
    }
    if (args == NULL) {
        return ;
    }
    int i = 0;
    while (args[i]!= NULL) {
        free(args[i]);
        ++i;
    }
    free(args);
}

void get_fork_args(int clnt_sock) {
    char* receive_args = connect_list[clnt_sock].recv_package->data;

    char* command;
    char** args;
    args = split_args(receive_args, &command);
    if (args == NULL) {
        error_handling(ERROR_NUMBER_FORKARGS_NULL, clnt_sock, NULL);
        return ;
    }

#if CHECK_ARGS
   check_args(command, args);
#endif

    int receive_gid = -1;
    int receive_pid = -1;
    int fork_group_parameter_index = -1;

    enum ERROR_NUMBER error_number = get_forkgroup_parameters(args, &receive_gid, &receive_pid, &fork_group_parameter_index);
    if (error_number) {
        error_handling(error_number, clnt_sock, NULL);
        return ;
    }

    // printf("receive_gid: %d, receive_pid: %d\n", receive_gid, receive_pid);

    // 1. receive_gid = 0, receive_pid = 1
    // 2. receive_gid = m, receive_pid = 1
    // 3. receive_gid = m, receive_pid = n (n > 1)

    // receive_gid must be legal
    if (receive_gid == 0) {
        printf("error : receive_gid = 0\n");
        exit(1);
    }

    int gid = receive_gid;
    if (!is_gid_set(&gpbmap, gid) || !is_pid_set(&gpbmap, gid, receive_pid)) {
        error_handling(ERROR_NUMBER_ILLEGAL_GIDPID, clnt_sock, NULL);
        free_args(command, args);
        return ;
    }

    int pid = find_free_pid(&gpbmap, gid);
    if (pid == -1) {
        error_handling(ERROR_NUMBER_NOFREEPID, clnt_sock, (void*)gid);
        free_args(command, args);
        return ;
    }

    connect_list[clnt_sock].next_child_pid = pid;

    // 仅返回子进程的 pid
    // 构造 send_package
    package* pack = connect_list[clnt_sock].send_package;
    pack->command = CMD_SENDGIDPID;
    char* temp = (char*)malloc(sizeof(char)*BUF_SIZE);
    pack->len = sprintf(temp, "%d", pid);
    pack->data = (char*)malloc(pack->len);
    memcpy(pack->data, temp, pack->len);

    sprintf(temp, "gid=%d,pid=%d", gid, pid);
    free(args[fork_group_parameter_index]);
    args[fork_group_parameter_index] = (char*)malloc(sizeof(char)*(strlen(temp)+1));
    strcpy(args[fork_group_parameter_index], temp);
    free(temp);

#if CHECK_ARGS_CHILD
    printf("child progress check args\n");
    check_args(command, args);
#endif

    connect_list[clnt_sock].command = command;
    connect_list[clnt_sock].args = args;
}

static inline int check_number(char* str) {
    if (str == NULL) {
        return 0;
    }
    return (str[0] >= '0' && str[0] <= '9') ? 1 : 0;
}

static int endsWith(const char*str, const char* suffix, const int suffixLen) {
    int strLen = strlen(str);
    if (suffixLen > strLen) {
        return 0; 
    }
    const char* ptr = str + (strLen - suffixLen);
    return strcmp(ptr, suffix) == 0;
}

static char** split_args(char* args, char** command) {
    char* p = strtok(args, " ");
    if (p == NULL) {
        printf("split_args() error.\n");
        return NULL;
    }
    char* comm = (char*)malloc(sizeof(char)*(strlen(p)+1));
    strcpy(comm, p);
    *command = comm;

    char** result = (char**)malloc(sizeof(char*)*ARG_NUMBER);
    result[0] = (char *)malloc((strlen(p) + 1) * sizeof(char));
    strcpy(result[0], p);

    int index = 1;
    p = strtok(NULL, " ");
    while (p!= NULL) {
        if (index >= ARG_NUMBER) {
            printf("split_args() error: too many args.\n");
            result[ARG_NUMBER - 1] = NULL;
            free_args(comm, result);
            return NULL;
        }

        // TODO: -append 参数合并
        // ugly：先处理 nginx 和 redis 应用参数均以 .conf 结尾的特征

        result[index] = (char *)malloc((strlen(p) + 1) * sizeof(char));
        strcpy(result[index], p);
        index++;

        if (strcmp(p, "-append") == 0) {
            char* append_parameter = (char*)malloc(sizeof(char)*(PARAMETER_SIZE+1));
            memset(append_parameter, 0, PARAMETER_SIZE+1);
            p = strtok(NULL, " ");
            // TODO : error handle
            while (p != NULL) {
                if (!endsWith(p, ".conf", 5)) {
                    strcat(append_parameter, p);
                    strcat(append_parameter, " ");
                    p = strtok(NULL, " ");
                } else {
                    strcat(append_parameter, p);
                    break;
                }
            }

            // printf("append_parameter : %s.\n", append_parameter);

            result[index] = (char *)malloc((strlen(append_parameter) + 1) * sizeof(char));
            strcpy(result[index], append_parameter);
            index++;
            free(append_parameter);

        }
        p = strtok(NULL, " ");
    }

    result[index] = NULL;

    return result;
}

static enum ERROR_NUMBER get_forkgroup_parameters(char** args, int* receive_gid, int* receive_pid, int* fork_group_parameter_index) {
    int index = 0;
    char* forkgroup_parameter = NULL;
    while (args[index] != NULL) {
        if (strcmp(args[index], "-forkgroup") == 0 && args[index+1] != NULL) {
            forkgroup_parameter = (char*)malloc(sizeof(char)*(strlen(args[index+1])+1));
            strcpy(forkgroup_parameter, args[++index]);
            break;
        }
        index ++;
    }

    if (forkgroup_parameter == NULL) {
        return ERROR_NUMBER_IVALID_ARGS;
    }

    char* p = strtok(forkgroup_parameter, "=,");
    p = strtok(NULL, "=,");
    if (check_number(p)) {
        *receive_gid = atoi(p);
    } else {
        free(forkgroup_parameter);
        return ERROR_NUMBER_IVALID_ARGS;
    }
    p = strtok(NULL, "=,");
    p = strtok(NULL, "=,");
    if (check_number(p)) {
        *receive_pid = atoi(p);
    } else {
        free(forkgroup_parameter);
        return ERROR_NUMBER_IVALID_ARGS;
    }

    *fork_group_parameter_index = index;
    free(forkgroup_parameter);
    return NUMBER_SUCCESS;
}

