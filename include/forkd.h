#ifndef __FORKD__
#define __FORKD__

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

void error_handling(char *message);

void set_noblocking_mode(int fd);

void set_event(int fd, int event, int flag);

#endif