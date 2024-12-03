#ifndef __SERVER__
#define __SERVER__

#include "package.h"

#define BUF_SIZE 1024
#define EPOLL_SIZE 50
#define WAIT_LIST_LEN 8

typedef void (*callback)(int);

#define RUN 0
#define ZOMBIE 1
#define DIED 2


typedef struct {
    int fd;
    char* command;
    char** args;
    int gid;
    int pid;
    int global_pid;
    int next_child_pid;
    int wait_count;
    int wait_list[WAIT_LIST_LEN];
    int statu;
    package* recv_package;
    package* send_package;
    callback s_recv_callback;
    callback s_send_callback;
} connect_item;

void start_server(int port_id);
void init_server(unsigned short port);
void accept_callback(int serv_sock);
void recv_callback(int clnt_sock);
void send_callback(int clnt_sock);

#endif