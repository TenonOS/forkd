#include "../include/server.h"
#include "../include/bitmap.h"
#include "../include/args_handle.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>

gpid_bitmap gpbmap;
int epfd = 0;
connect_item connect_list[EPOLL_SIZE+3]; // 连接列表

static void set_noblocking_mode(int fd);

void start_server(int port_id) {
    init_gpid_bitmap(&gpbmap);
    epfd = epoll_create(EPOLL_SIZE);
    struct epoll_event* ep_events = (struct epoll_event*)malloc(sizeof(struct epoll_event) * EPOLL_SIZE);
    init_server(port_id);
    while (1) {
        int event_cnt = epoll_wait(epfd, ep_events, EPOLL_SIZE, -1);
        if (event_cnt == -1) {
            if (errno == EINTR) {
                continue;
            } else {
                printf("epoll_wait() error.\n");
                break;
            }
            printf("epoll_wait() error.\n");
        } else {
            // printf("event_cnt: %d\n", event_cnt);
        }

        for (int i = 0; i < event_cnt; i++) {
            int fd = ep_events[i].data.fd;
            // printf("fd: %d\n", fd);
            if (ep_events[i].events & EPOLLIN) {
                connect_list[fd].s_recv_callback(fd);
            } else if (ep_events[i].events & EPOLLOUT) {
                connect_list[fd].s_send_callback(fd);
            }
        }
    }
}

void init_server(unsigned short port) {
    int serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (serv_sock == -1) {
        printf("socket() error.\n");
    }

    int option = 1;
    int optlen = sizeof(option);
    setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, (void*)&option, optlen);
    
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);

    if (bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
        printf("bind() error.\n");
    }

    if (listen(serv_sock, 1) == -1) {
        printf("listen() error.\n");
    }

    connect_list[serv_sock].fd = serv_sock;
    connect_list[serv_sock].s_recv_callback = accept_callback;

    set_noblocking_mode(serv_sock);
    set_event(serv_sock, EPOLLIN, 1);
}

void accept_callback(int serv_sock) {
    struct sockaddr_in clnt_addr;
    int clnt_addr_size = sizeof(clnt_addr);
    int clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);
    if (clnt_sock == -1) {
        printf("accept() error.\n");
    } else {
        // printf("accept client: %d\n", clnt_sock);
    }

    connect_list[clnt_sock].fd = clnt_sock;
    connect_list[clnt_sock].flag = 0;
    connect_list[clnt_sock].command = NULL;
    connect_list[clnt_sock].args = NULL;
    connect_list[clnt_sock].s_recv_callback = recv_callback;
    connect_list[clnt_sock].s_send_callback = send_callback;
    memset(connect_list[clnt_sock].rbuffer, 0, BUF_SIZE);
    memset(connect_list[clnt_sock].wbuffer, 0, BUF_SIZE);
    connect_list[clnt_sock].rindex = 0;
    connect_list[clnt_sock].windex = 0;

    // memcpy(connect_list[clnt_sock].wbuffer, "fork receive", 12);
    // connect_list[clnt_sock].windex = 12;

    set_noblocking_mode(clnt_sock);
    set_event(clnt_sock, EPOLLIN|EPOLLET, 1);
}

void recv_callback(int clnt_sock) {
    if (!connect_list[clnt_sock].flag) {
        // printf("get fork args.\n");
        get_fork_args(clnt_sock);
        return ;
    }

    close(clnt_sock); 

    if (connect_list[clnt_sock].command == NULL || connect_list[clnt_sock].args == NULL)
        return ;

    // printf("qeum fork finished, do fork.\n");

    pid_t tmp_pid;
    tmp_pid = vfork();
    if (tmp_pid != 0) {
        // parent progress
        // printf("parent progress\n");
        // TODO free command and args
        free_args(connect_list[clnt_sock].command, connect_list[clnt_sock].args);
        // waitpid(-1, NULL, 0);
    } else {
        // child progress
        // printf("child progress\n");
        // check_args(connect_list[clnt_sock].command, connect_list[clnt_sock].args);        
        execvp(connect_list[clnt_sock].command, connect_list[clnt_sock].args);
        exit(0);
    }
}

void send_callback(int clnt_sock) {
    write(clnt_sock, connect_list[clnt_sock].wbuffer, connect_list[clnt_sock].windex);
    shutdown(clnt_sock, SHUT_WR);   // half close
    set_event(clnt_sock, EPOLLIN|EPOLLET, 0);
}

static void set_noblocking_mode(int fd) {
    int flag = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flag | O_NONBLOCK);
}

void set_event(int fd, int event, int flag) {
	if (flag) { // 1 add, 0 mod
		struct epoll_event ev;
		ev.events = event ;
		ev.data.fd = fd;
		epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
	} else {
		struct epoll_event ev;
		ev.events = event;
		ev.data.fd = fd;
		epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
	}
}