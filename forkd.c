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

#define ERROR_MESSAGE_SIZE 512
#define PARAMETER_SIZE 128

// TODO 暂不处理释放逻辑

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

static void check_args(char* command, char** args);
static void free_args(char* command, char** args);
static int mask_pid(int gid);
static int get_forkgroup_parameters(char** args, int* receive_gid, int* receive_pid, int* fork_group_parameter_index);
static inline int check_number(char* str);
static void get_fork_args(int clnt_sock);

void recv_callback(int clnt_sock) {
    if (!connect_list[clnt_sock].flag) {
        printf("get fork args.\n");
        get_fork_args(clnt_sock);
        return ;
    }

    close(clnt_sock); 

    if (connect_list[clnt_sock].command == NULL || connect_list[clnt_sock].args == NULL)
        return ;

    printf("qeum fork finished, do fork.\n");

    pid_t tmp_pid;
    tmp_pid = vfork();
    if (tmp_pid != 0) {
        // parent progress
        // printf("parent progress\n");
        // TODO free command and args
        free_args(connect_list[clnt_sock].command, connect_list[clnt_sock].args);
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

void error_handling(int error_number, int clnt_sock, void* data) {
    char* error_message = NULL;
    int malloc_flag = 0;
    switch (error_number)
    {
    case 1:
        error_message = "error: -forkgroup is not found or parameter is invalid.";
        break;
    case 2:
        error_message = "error: -forkgroup parameter is invalid.";
        break;
    case 3:
        error_message = (char*)malloc(sizeof(char)*ERROR_MESSAGE_SIZE);
        sprintf(error_message, "fatal error: pid_bitmap of gid %d is dirty.", (int)data);
        malloc_flag = 1;
        break;
    case 4:
        error_message = "error: args is NULL.";
        break;
    case 5:
        error_message = "error: -forkgroup parameter is invalid, if gid == 0, pid must be 1.";
        break;
    case 6:
        error_message = "error: no gid available.";
        break;
    case 7:
        error_message = "error: gid or pid is not available.";
        break;
    case 8:
        error_message = (char*)malloc(sizeof(char)*ERROR_MESSAGE_SIZE);
        sprintf(error_message, "error: no pid available of group %d.", (int)data);
        malloc_flag = 1;
        break;
    
    default:
        break;
    }
    printf("%s\n", error_message);
    memcpy(connect_list[clnt_sock].wbuffer, error_message, strlen(error_message));
    connect_list[clnt_sock].windex = strlen(error_message);
    set_event(clnt_sock, EPOLLOUT|EPOLLET, 0);
    if (malloc_flag) {
        free(error_message);
    }
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
            result[ARG_NUMBER - 1] = NULL;
            free_args(comm, result);
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

static void check_args(char* command, char** args) {
    printf("check args\n");
    printf("command: %s\n", command);
    int i = 0;
    while (args[i] != NULL) {
        printf("args[%d]: %s\n", i, args[i]);
        ++i;
    }
    printf("check args end\n");
}

static void free_args(char* command, char** args) {
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

static inline int check_number(char* str) {
    if (str == NULL) {
        return 0;
    }
    return (str[0] >= '0' && str[0] <= '9') ? 1 : 0;
}

static int get_forkgroup_parameters(char** args, int* receive_gid, int* receive_pid, int* fork_group_parameter_index) {
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
        return 1;
    }

    char* p = strtok(forkgroup_parameter, "=,");
    p = strtok(NULL, "=,");
    if (check_number(p)) {
        *receive_gid = atoi(p);
    } else {
        free(forkgroup_parameter);
        return 2;
    }
    p = strtok(NULL, "=,");
    p = strtok(NULL, "=,");
    if (check_number(p)) {
        *receive_pid = atoi(p);
    } else {
        free(forkgroup_parameter);
        return 2;
    }

    *fork_group_parameter_index = index;
    free(forkgroup_parameter);
    return NULL;
}

static int mask_pid(int gid) {
    // pid 0 is illegal
    int pid0 = find_free_pid(&gpbmap, gid);
    // pid 1 need set
    int pid1 = find_free_pid(&gpbmap, gid);

    if (pid0 != 0 || pid1 != 1) {
        release_gid(&gpbmap, gid);
        return 3;
    }
    return 0;
}

static void get_fork_args(int clnt_sock) {
    connect_list[clnt_sock].flag = 1;
    while (1) {
        if (connect_list[clnt_sock].rindex >= BUF_SIZE) {
            printf("recv_callback() error, out of buffer.\n");
        }
        int str_len = read(clnt_sock, connect_list[clnt_sock].rbuffer+connect_list[clnt_sock].rindex, BUF_SIZE-connect_list[clnt_sock].rindex);
        if (str_len == 0) {
            epoll_ctl(epfd, EPOLL_CTL_DEL, clnt_sock, NULL);
            close(clnt_sock);
            // printf("close client: %d\n", clnt_sock);
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

    if (args == NULL) {
        error_handling(4, clnt_sock, NULL);
        return ;
    }

// #define CHECK_ARGS
#ifdef CHECK_ARGS
   check_args(command, args);
#endif

    int receive_gid = -1;
    int receive_pid = -1;
    int fork_group_parameter_index = -1;

    int error_number = get_forkgroup_parameters(args, &receive_gid, &receive_pid, &fork_group_parameter_index);
    if (error_number) {
        error_handling(error_number, clnt_sock, NULL);
        return ;
    }

    printf("receive_gid: %d, receive_pid: %d\n", receive_gid, receive_pid);

    // 1. receive_gid = 0, receive_pid = 1
    // 2. receive_gid = m, receive_pid = 1
    // 3. receive_gid = m, receive_pid = n (n > 1)
    
    int gid = -1;
    
    if (receive_gid == 0) {
        if (receive_pid != 1) {
            error_handling(5, clnt_sock, NULL);
            free_args(command, args);
            // TODO free command and args
            return ;
        }
        gid = find_free_gid(&gpbmap);
        if (gid == -1) {
            error_handling(6, clnt_sock, NULL);
            free_args(command, args);
            return ;
        }

        error_number = mask_pid(gid);
        if (error_number) {
            error_handling(error_number, clnt_sock, (void*)gid);
            free_args(command, args);
            return ;
        }
    } else {
        gid = receive_gid;
        if (!is_gid_set(&gpbmap, gid) || !is_pid_set(&gpbmap, gid, receive_pid)) {
            error_handling(7, clnt_sock, NULL);
            free_args(command, args);
            return ;
        }
    }

    int pid = find_free_pid(&gpbmap, gid);
    if (pid == -1) {
        error_handling(8, clnt_sock, (void*)gid);
        return ;
    }

    // printf("new_gid: %d, new_pid: %d\n", gid, pid);

    // 规定：返回信息中第一个参数为 gid，第二个参数为 pid
    connect_list[clnt_sock].windex = sprintf(connect_list[clnt_sock].wbuffer, "%d %d", gid, pid);        
    set_event(clnt_sock, EPOLLOUT|EPOLLET, 0);

    char* temp = (char*)malloc(sizeof(char)*BUF_SIZE);
    sprintf(temp, "gid=%d,pid=%d", gid, pid);
    free(args[fork_group_parameter_index]);
    args[fork_group_parameter_index] = (char*)malloc(sizeof(char)*(strlen(temp)+1));
    strcpy(args[fork_group_parameter_index], temp);
    free(temp);

// #define CHECK_ARGS_CHILD
#ifdef CHECK_ARGS_CHILD
    printf("child progress check args\n");
    check_args(command, args);
#endif

    // connect_list[clnt_sock].flag = 1;
    connect_list[clnt_sock].command = command;
    connect_list[clnt_sock].args = args;
}