#include "../include/server.h"
#include "../include/bitmap.h"
#include "../include/args_handle.h"
#include "../include/package.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>

int gpid[128][128] = {0};

gpid_bitmap gpbmap;
int epfd = 0;
connect_item connect_list[EPOLL_SIZE+3]; // 连接列表

static void set_event(int fd, int event, int flag);
static void set_noblocking_mode(int fd);
static void init_connect_item(int clnt_sock);
static void clear_connect_item(int clnt_sock);

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
        }

        for (int i = 0; i < event_cnt; i++) {
            int fd = ep_events[i].data.fd;
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
    }

    init_connect_item(clnt_sock);
    set_noblocking_mode(clnt_sock);
    set_event(clnt_sock, EPOLLIN|EPOLLET, 1);
}


static void receive_package(int clnt_sock);
static void handle_init(int clnt_sock);
static void do_fork(int clnt_sock);
static void handle_client_close(int clnt_sock);
static void handle_waitpid(int clnt_sock);
static void handle_kill(int clnt_sock);

void recv_callback(int clnt_sock) {
    receive_package(clnt_sock);
    check_package(connect_list[clnt_sock].recv_package);

    switch (connect_list[clnt_sock].recv_package->command) {
        case CMD_INIT:
            handle_init(clnt_sock);
            break;
        case CMD_FORKPARA:
            get_fork_args(clnt_sock);
            set_event(clnt_sock, EPOLLOUT|EPOLLET, 0);
            break;
        case CMD_QEMUFORK:
            do_fork(clnt_sock);
            break;
        case CMD_CLIENTCOLSE:
            handle_client_close(clnt_sock);
            break;
        case CMD_WAITPID:
            handle_waitpid(clnt_sock);
            break;
        case CMD_KILL:
            handle_kill(clnt_sock);
            break;
        default:
            return ;
    }

    if (connect_list[clnt_sock].recv_package->data!= NULL) {
        free(connect_list[clnt_sock].recv_package->data);
        connect_list[clnt_sock].recv_package->data = NULL;
    }

    return ;
}

void send_callback(int clnt_sock) {
    int str_len;
    char* message = serialize_package(connect_list[clnt_sock].send_package, &str_len);
    write(clnt_sock, message, str_len);
    free(message);
    if (connect_list[clnt_sock].send_package->data!= NULL) {
        free(connect_list[clnt_sock].send_package->data);
        connect_list[clnt_sock].send_package->data = NULL;
    }
    set_event(clnt_sock, EPOLLIN|EPOLLET, 0);
}

static void set_event(int fd, int event, int flag) {
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

static void set_noblocking_mode(int fd) {
    int flag = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flag | O_NONBLOCK);
}

static void readlen(int clnt_sock, char* buffer, int len) {
    int str_len = 0;
    while (str_len < len) {
        int read_len = read(clnt_sock, buffer+str_len, len-str_len);
        if (read_len == 0) {
            printf("client close.\n");
            return ;
        } else if (read_len < 0) {
            if (errno == EAGAIN) {
                break;
            }
        } else {
            str_len += read_len;
        }
    }
    return ;
}

static void receive_package(int clnt_sock) {
    package* pack = connect_list[clnt_sock].recv_package;
    int cmd_len = sizeof(pack->command);
    // 读取 CMD
    readlen(clnt_sock, (char*)&pack->command, cmd_len);
    // printf("cmd: %d\n", pack->command);
    // 读取 DATA_LEN
    int args_len = sizeof(pack->len);
    readlen(clnt_sock, (char*)&pack->len, args_len);
    // printf("len: %d\n", pack->len);
    // 读取 DATA
    if (pack->len > 0) {
        pack->data = (char*)malloc(pack->len);
        readlen(clnt_sock, pack->data, pack->len);
    }
    
    return ;
}

static void handle_init(int clnt_sock) {
    char* p = connect_list[clnt_sock].recv_package->data;
    // TODO : error handle
    p = strtok(p, " ");
    int gid = atoi(p);
    p = strtok(NULL, " ");
    int pid = atoi(p);

    int error_flag = 0;

    if (gid == 0) {
        do {
            if (pid != 1) {
                error_handling(ERROR_NUMBER_IVALID_ARGS, clnt_sock, NULL);
                error_flag = 1;
                break;
            }
            gid = find_free_gid(&gpbmap);
            if (gid == -1) {
                error_handling(ERROR_NUMBER_NOFREEGID, clnt_sock, NULL);
                error_flag = 1;
                break;
            }
            int error_number = mask_pid(gid, &gpbmap);
            if (error_number) {
                error_handling(error_number, clnt_sock, (void*)gid);
                error_flag = 1;
                break; 
            }
        } while(0);
    
        if (error_flag) {
            set_event(clnt_sock, EPOLLOUT|EPOLLET, 0);
            return ;
        }

        package* pack = connect_list[clnt_sock].send_package;
        pack->command = CMD_INITRET;
        char* temp = (char*)malloc(sizeof(char)*BUF_SIZE);
        pack->len = sprintf(temp, "%d", gid);
        pack->data = (char*)malloc(pack->len);
        memcpy(pack->data, temp, pack->len);
        free(temp);
        set_event(clnt_sock, EPOLLOUT|EPOLLET, 0);
    }

    connect_list[clnt_sock].gid = gid;
    connect_list[clnt_sock].pid = pid;
    connect_list[clnt_sock].global_pid = gpid[gid][pid];
    gpid[gid][pid] = clnt_sock; 
}

static void do_fork(int clnt_sock) {
    if (connect_list[clnt_sock].command == NULL || connect_list[clnt_sock].args == NULL)
        return ;

    pid_t tmp_pid;
    tmp_pid = vfork();

    if (tmp_pid != 0) {
        free_args(connect_list[clnt_sock].command, connect_list[clnt_sock].args);
        gpid[connect_list[clnt_sock].gid][connect_list[clnt_sock].next_child_pid] = tmp_pid;
        // waitpid(-1, NULL, 0);
    } else {
        // check_args(connect_list[clnt_sock].command, connect_list[clnt_sock].args);        
        execvp(connect_list[clnt_sock].command, connect_list[clnt_sock].args);
        exit(0);
    }
}

static void handle_client_close(int clnt_sock) {
    // 删除事件
    epoll_ctl(epfd, EPOLL_CTL_DEL, clnt_sock, NULL);
    int count  = connect_list[clnt_sock].wait_count;
    int* wait_list = connect_list[clnt_sock].wait_list;
    if (count > 0) {
        int* wait_list = connect_list[clnt_sock].wait_list;
        for (int i = 0; i < count; i++) {
            int call_sock = wait_list[i];
            // TODO 发送消息给等待的进程
            package* pack = connect_list[call_sock].send_package;
            pack->command = CMD_WAITPIDRET;
            pack->len = 0;
            pack->data = NULL;
            set_event(call_sock, EPOLLOUT|EPOLLET, 0);
        }
        gpid[connect_list[clnt_sock].gid][connect_list[clnt_sock].pid] = 0;
        clear_connect_item(clnt_sock);
        close(clnt_sock);
    } else {
        connect_list[clnt_sock].statu = ZOMBIE;
    }
}

static void handle_waitpid(int clnt_sock) {
    char* p = connect_list[clnt_sock].recv_package->data;
    p = strtok(p, " ");
    int gid = atoi(p);
    p = strtok(NULL, " ");
    int pid = atoi(p);

    int sock = gpid[gid][pid];
    if (sock == 0) {
        // TODO : error handle
        printf("error: pid not found.\n");
    }

    if (connect_list[sock].statu == ZOMBIE) {
        package* pack = connect_list[clnt_sock].send_package;
        pack->command = CMD_WAITPIDRET;
        pack->len = 0;
        pack->data = NULL;
        gpid[gid][pid] = 0;
        clear_connect_item(sock);
        close(sock);
        set_event(clnt_sock, EPOLLOUT|EPOLLET, 0);
    } else {
        connect_list[sock].wait_list[connect_list[sock].wait_count++] = clnt_sock;
    }
}

static void handle_kill(int clnt_sock) {
    char* p = connect_list[clnt_sock].recv_package->data;
    p = strtok(p, " ");
    int gid = atoi(p);
    p = strtok(NULL, " ");
    int pid = atoi(p);

    int sock = gpid[gid][pid];
    if (sock == 0) {
        // TODO : error handle
        printf("error: pid not found.\n");
    }

    kill(connect_list[sock].global_pid, SIGKILL);

    gpid[gid][pid] = 0;
    epoll_ctl(epfd, EPOLL_CTL_DEL, clnt_sock, NULL);
    clear_connect_item(sock);
    close(sock);
}

static void init_connect_item(int clnt_sock) {
    connect_list[clnt_sock].fd = clnt_sock;
    connect_list[clnt_sock].command = NULL;
    connect_list[clnt_sock].args = NULL;
    connect_list[clnt_sock].gid = -1;
    connect_list[clnt_sock].pid = -1;
    connect_list[clnt_sock].global_pid = -1;
    connect_list[clnt_sock].wait_count = 0;
    connect_list[clnt_sock].statu = -1;
    memset(connect_list[clnt_sock].wait_list, 0, sizeof(int) * WAIT_LIST_LEN);
    connect_list[clnt_sock].s_recv_callback = recv_callback;
    connect_list[clnt_sock].s_send_callback = send_callback;
    connect_list[clnt_sock].recv_package = (package*)malloc(sizeof(package));
    connect_list[clnt_sock].send_package = (package*)malloc(sizeof(package));
}

static void clear_connect_item(int clnt_sock) {
    connect_list[clnt_sock].fd = -1;
    connect_list[clnt_sock].command = NULL;
    connect_list[clnt_sock].args = NULL;
    connect_list[clnt_sock].gid = -1;
    connect_list[clnt_sock].pid = -1;
    connect_list[clnt_sock].global_pid = -1;
    connect_list[clnt_sock].wait_count = 0;
    memset(connect_list[clnt_sock].wait_list, 0, sizeof(int) * WAIT_LIST_LEN);
    connect_list[clnt_sock].s_recv_callback = NULL;
    connect_list[clnt_sock].s_send_callback = NULL;
    free(connect_list[clnt_sock].recv_package);
    free(connect_list[clnt_sock].send_package);
    connect_list[clnt_sock].recv_package = NULL;
    connect_list[clnt_sock].send_package = NULL;
}