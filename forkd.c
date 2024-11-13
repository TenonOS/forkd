#include "./include/forkd.h"
#include "./include/bitmap.h"
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

#define ERROR_MESSAGE_SIZE 128

static gpid_bitmap gpbmap;

static int epfd = 0;

static connect_item connect_list[EPOLL_SIZE+3]; // 连接列表

int main(int argc, char *argv[]) {
    if (argc!= 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    init_gpid_bitmap(&gpbmap);

    epfd = epoll_create(EPOLL_SIZE);
    struct epoll_event* ep_events = (struct epoll_event*)malloc(sizeof(struct epoll_event) * EPOLL_SIZE);

    init_server(atoi(argv[1]));

    while (1) {
        int event_cnt = epoll_wait(epfd, ep_events, EPOLL_SIZE, -1);
        if (event_cnt == -1) {
            printf("epoll_wait() error.\n");
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
        printf("accept client: %d\n", clnt_sock);
    }

    connect_list[clnt_sock].fd = clnt_sock;
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

static inline int check_number(char* str) {
    if (str == NULL) {
        return 0;
    }
    return (str[0] >= '0' && str[0] <= '9') ? 1 : 0;
}

static inline char* get_forkgroup_parameters(char** args, int* receive_gid, int* receive_pid) {
    char* forkgroup_parameter = NULL;
    int index = 0;
    while (args[index] != NULL) {
        if (strcmp(args[index], "-forkgroup") == 0) {
            forkgroup_parameter = (char*)malloc(sizeof(char)*(strlen(args[index+1])+1));
            strcpy(forkgroup_parameter, args[index+1]);
            break;
        }
        index ++;
    }

    if (forkgroup_parameter == NULL) {
        char* error_message = "error: -forkgroup is not found or parameter is invalid.";
        return error_message;
    }

    char* p = strtok(forkgroup_parameter, "=,");
    p = strtok(NULL, "=,");
    if (check_number(p)) {
        *receive_gid = atoi(p);
    } else {
        char* error_message = "error: -forkgroup parameter is invalid.";
        return error_message;
    }
    p = strtok(NULL, "=,");
    p = strtok(NULL, "=,");
    if (check_number(p)) {
        *receive_pid = atoi(p);
    } else {
        char* error_message = "error: -forkgroup parameter is invalid.";
        return error_message;
    }

    return NULL;
}

static inline char* mask_pid(int gid) {
    // pid 0 is illegal
    int pid0 = find_free_pid(&gpbmap, gid);
    // pid 1 need set
    int pid1 = find_free_pid(&gpbmap, gid);

    if (pid0 != 0 || pid1 != 1) {
        release_gid(&gpbmap, gid);
        char* error_message = (char*)malloc(sizeof(char)*BUF_SIZE);
        sprintf(error_message, "fatal error: pid_bitmap of gid %d is dirty.", gid);
        return error_message;
    }

    return NULL;
}

void recv_callback(int clnt_sock) {
    while (1) {
        if (connect_list[clnt_sock].rindex >= BUF_SIZE) {
            printf("recv_callback() error, out of buffer.\n");
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
    connect_list[clnt_sock].rbuffer[connect_list[clnt_sock].rindex] = '\0';

    char* command;
    char** args;
    args = split_args(connect_list[clnt_sock].rbuffer, &command);
    memset(connect_list[clnt_sock].rbuffer, 0, BUF_SIZE);
    connect_list[clnt_sock].rindex = 0;

#define CHECK_ARGS
#ifdef CHECK_ARGS
    printf("command: %s\n", command);
    int i = 0;
    while (args[i] != NULL) {
        printf("args[%d]: %s\n", i, args[i]);
        ++i;
    }
#endif

    int receive_gid = -1;
    int receive_pid = -1;

    char* error_message = get_forkgroup_parameters(args, &receive_gid, &receive_pid);
    if (error_message!= NULL) {
        error_handling(error_message, clnt_sock);
        return ;
    }

    printf("receive_gid: %d, receive_pid: %d\n", receive_gid, receive_pid);

    // 1. receive_gid = 0, receive_pid = 1
    // 2. receive_gid = m, receive_pid = 1
    // 3. receive_gid = m, receive_pid = n (n > 1)
    
    int gid = -1;
    
    if (receive_gid == 0) {
        if (receive_pid != 1) {
            char* error_message = "error: -forkgroup parameter is invalid, if gid == 0, pid must be 1.";
            error_handling(error_message, clnt_sock);
            return ;
        }
        gid = find_free_gid(&gpbmap);
        if (gid == -1) {
            char* error_message = "error: no gid available.";
            error_handling(error_message, clnt_sock);
            return ;
        }

        char* error_message = mask_pid(gid);
        if (error_message != NULL) {
            error_handling(error_message, clnt_sock);
            return ;
        }

        // pid = find_free_pid(&gpbmap, gid);
    } else {
        gid = receive_gid;
        if (!is_gid_set(&gpbmap, gid) || !is_pid_set(&gpbmap, gid, receive_pid)) {
            char error_message[ERROR_MESSAGE_SIZE];
            sprintf(error_message, "error: gid %d or pid %d is not available.", gid, receive_pid);
            error_handling(error_message, clnt_sock);
            return ;
        }
    }

    int pid = find_free_pid(&gpbmap, gid);
    if (pid == -1) {
        char error_message[ERROR_MESSAGE_SIZE];
        sprintf(error_message, "error: no pid available of group %d.", gid);
        error_handling(error_message, clnt_sock);
        return ;
    }

    pid_t tmp_pid;
    tmp_pid = vfork();
    if (tmp_pid != 0) {
        // father progress
        // tmp, read and write client information use one buffer
        // write(fileno(stdout), connect_list[clnt_sock].rbuffer, connect_list[clnt_sock].rindex);
        // putchar('\n');
        // memset(connect_list[clnt_sock].rbuffer, 0, BUF_SIZE);
        // connect_list[clnt_sock].rindex = 0;

        // 规定：返回信息中第一个参数为 gid，第二个参数为 pid

        connect_list[clnt_sock].windex = sprintf(connect_list[clnt_sock].wbuffer, "%d %d", gid, pid);

        // if (forkgroup->parameter == NULL) {
        //     connect_list[clnt_sock].windex = sprintf(connect_list[clnt_sock].wbuffer, "%d %d", gid, pid);
        // } else {
        //     connect_list[clnt_sock].windex = sprintf(connect_list[clnt_sock].wbuffer, "%d", pid);
        // }
        
        set_event(clnt_sock, EPOLLOUT|EPOLLET, 0);
    } else {
        // child progress
        // printf("child progress\n");
        execvp(command, args);
        exit(0);
    }
}

void send_callback(int clnt_sock) {
    write(clnt_sock, connect_list[clnt_sock].wbuffer, connect_list[clnt_sock].windex);
    shutdown(clnt_sock, SHUT_WR);   // half close
    set_event(clnt_sock, EPOLLIN|EPOLLET, 0);
}

void error_handling(char *message, int clnt_sock) {
    printf("%s\n", message);
    memcpy(connect_list[clnt_sock].wbuffer, message, strlen(message));
    connect_list[clnt_sock].windex = strlen(message);
    set_event(clnt_sock, EPOLLOUT|EPOLLET, 0);
    return ;
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

#define ARG_NUMBER 128

char** split_args(char* args, char** command) {
    char* p = strtok(args, " ");
    if (p == NULL) {
        printf("split_args() error.\n");
        return NULL;
    }
    char* comm = (char*)malloc(sizeof(char)*(strlen(p)+1));
    strcpy(comm, p);
    *command = comm;

    p = strtok(NULL, " ");
    int index = 0;  
    char** result = (char**)malloc(sizeof(char*)*ARG_NUMBER);

    while (p!= NULL) {
        if (index >= ARG_NUMBER) {
            printf("split_args() error: too many args.\n");
            return NULL;
        }
        result[index] = (char *)malloc((strlen(p) + 1) * sizeof(char));
        strcpy(result[index], p);
        index++;
        p = strtok(NULL, " ");
    }

    result[index] = NULL;

    return result;
}