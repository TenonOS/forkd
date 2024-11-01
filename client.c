#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>


int main() {
    char *request = "fork request";
    uint16_t serv_port = 9190;
    const char* serv_ip = "127.0.0.1";
    int serv_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(serv_ip);
    serv_addr.sin_port = htons(serv_port);
    
    if(connect(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
        printf("Connect failed\n");
        exit(1);
    } else {
        printf("Connect success\n");
    }

    /* Send request to forkd */
    // request = parse_args_to_str();
    write(serv_sock, request, strlen(request));

    int buffer_size = 1024;
    char receive[buffer_size];
    int rindex = 0;
    memset(receive, 0, buffer_size);

    while (1) {
        int str_len = read(serv_sock, receive+rindex, buffer_size - rindex);
        if (str_len == 0) {
            close(serv_sock);
            break;
        } else {
            rindex += str_len;
            if (rindex >= buffer_size) {
                printf("Receive buffer is full\n");
                break;
            }
        }
    }

    write(fileno(stdout), receive, rindex);
    putchar('\n');

    // getchar();
}