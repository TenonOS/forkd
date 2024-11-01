#include "./include/forkd.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <linux/types.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>

static int epfd = 0;

static connect_item connect_list[EPOLL_SIZE+3]; // 连接列表

int main(int argc, char *argv[]) {
    if (argc!= 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    epfd = epoll_create(EPOLL_SIZE);
    struct epoll_event* ep_events = (struct epoll_event*)malloc(sizeof(struct epoll_event) * EPOLL_SIZE);

    init_server(atoi(argv[1]));

    while (1) {
        int event_cnt = epoll_wait(epfd, ep_events, EPOLL_SIZE, -1);
        if (event_cnt == -1) {
            error_handling("epoll_wait() error");
            break;
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


    // 逻辑放到 recv_callback 中
    pid_t pid;
    pid = vfork();
    if(pid != 0){
        // father progress

    } else {
        // child progress

    }
}

void init_server(unsigned short port) {
    int serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (serv_sock == -1) {
        error_handling("socket() error");
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
        error_handling("bind() error");
    }

    if (listen(serv_sock, 1) == -1) {
        error_handling("listen() error");
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
        error_handling("accept() error");
    } else {
        printf("accept client: %d\n", clnt_sock);
    }

    connect_list[clnt_sock].fd = clnt_sock;
    connect_list[clnt_sock].s_recv_callback = recv_callback;
    connect_list[clnt_sock].s_send_callback = send_callback;
    memset(connect_list[clnt_sock].rbuffer, 0, BUF_SIZE);
    memset(connect_list[clnt_sock].wbuffer, 0, BUF_SIZE);
    connect_list[clnt_sock].rindex = 0;
    connect_list[clnt_sock].windex = 0;

    memcpy(connect_list[clnt_sock].wbuffer, "fork receive", 12);
    connect_list[clnt_sock].windex = 12;

    set_noblocking_mode(clnt_sock);
    set_event(clnt_sock, EPOLLIN|EPOLLET, 1);
}

void recv_callback(int clnt_sock) {
    // tmp, read and write use one buffer
    while (1) {
        if (connect_list[clnt_sock].rindex >= BUF_SIZE) {
            error_handling("recv_callback() error, out of buffer");
        }
        int str_len = read(clnt_sock, connect_list[clnt_sock].rbuffer+connect_list[clnt_sock].rindex, BUF_SIZE-connect_list[clnt_sock].rindex);
        if (str_len == 0) {
            epoll_ctl(epfd, EPOLL_CTL_DEL, clnt_sock, NULL);
            close(clnt_sock);
            printf("close client: %d\n", clnt_sock);
            return ;
        } else if (str_len < 0) {
            if (errno == EAGAIN) {
                break;
            }
        } else {
            connect_list[clnt_sock].rindex += str_len;
        }
    }

    pid_t pid;
    pid = vfork();
    if(pid != 0){
        // father progress
        write(fileno(stdout), connect_list[clnt_sock].rbuffer, connect_list[clnt_sock].rindex);
        putchar('\n');
        memset(connect_list[clnt_sock].rbuffer, 0, BUF_SIZE);
        connect_list[clnt_sock].rindex = 0;
        set_event(clnt_sock, EPOLLOUT|EPOLLET, 0);
    } else {
        // child progress
        printf("child progress\n");
        exit(0);
    }
}

void send_callback(int clnt_sock) {
    write(clnt_sock, connect_list[clnt_sock].wbuffer, connect_list[clnt_sock].windex);
    shutdown(clnt_sock, SHUT_WR);   // half close
    set_event(clnt_sock, EPOLLIN|EPOLLET, 0);
}

void error_handling(char *message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

void set_noblocking_mode(int fd) {
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