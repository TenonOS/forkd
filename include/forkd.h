#ifndef __FORKD__
#define __FORKD__

#include "./include/list.h"

#define BUF_SIZE 1024
#define EPOLL_SIZE 50

typedef void (*callback)(int);

void accept_callback(int serv_sock);
void recv_callback(int clnt_sock);
void send_callback(int clnt_sock);

typedef struct {
    int fd;
    char rbuffer[BUF_SIZE];
    int rindex;
    char wbuffer[BUF_SIZE];
    int windex;
    callback s_recv_callback;
    callback s_send_callback;
} connect_item;

void init_server(unsigned short port);

void error_handling(char *message, int clnt_sock);

void set_noblocking_mode(int fd);

void set_event(int fd, int event, int flag);

typedef struct {
    list_h_t list_node;
    char* option;
    char* parameter;
} fargs;

typedef list_h_t fargs_list_node;

fargs_list_node* split_args(char* args);

#endif