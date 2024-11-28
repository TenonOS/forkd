#ifndef __SERVER__
#define __SERVER__

#define BUF_SIZE 1024
#define EPOLL_SIZE 50

typedef void (*callback)(int);

typedef struct {
    int fd;
    int flag; // if args received
    char* command;
    char** args;
    char rbuffer[BUF_SIZE];
    int rindex;
    char wbuffer[BUF_SIZE];
    int windex;
    callback s_recv_callback;
    callback s_send_callback;
} connect_item;

void start_server(int port_id);
void init_server(unsigned short port);
void accept_callback(int serv_sock);
void recv_callback(int clnt_sock);
void send_callback(int clnt_sock);
void set_event(int fd, int event, int flag);

#endif