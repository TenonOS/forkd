#include "./include/server.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
    if (argc!= 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    start_server(atoi(argv[1]));

    return 0;
}

// SIGCHLD 信号处理程序
// void sigchld_handler(int signum) {
//     // 使用 WNOHANG 选项调用 waitpid，使其不阻塞
//     // while (waitpid(-1, NULL, WNOHANG) > 0);
//     printf("child process exit.\n");
//     waitpid(-1, NULL, 0);
// }

// struct sigaction sa;
// sa.sa_handler = sigchld_handler;
// sigemptyset(&sa.sa_mask);
// sa.sa_flags = 0; // 确保被中断的系统调用自动重启

// // 设置 SIGCHLD 信号处理程序
// if (sigaction(SIGCHLD, &sa, NULL) == -1) {
//     perror("sigaction");
//     exit(EXIT_FAILURE);
// }