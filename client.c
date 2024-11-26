#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>


int main(int argc, char *argv[]) {
    if (argc!= 3) {
        printf("Usage:./client <gid> <pid>\n");
        exit(1);
    }
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

    char* request = "sudo qemu-system-x86_64 \
            -kernel \"$kernel\" \
            -enable-kvm \
            -nographic \
            -m 64M \
            -netdev bridge,id=en0,br=virbr0 -device virtio-net-pci,netdev=en0 \
            -append netdev.ip=172.44.0.2/24:172.44.0.1 -- -c /nginx/conf/nginx.conf \
            -fsdev local,id=myid,path=\"$rootfs\",security_model=none \
            -device virtio-9p-pci,fsdev=myid,mount_tag=test,disable-modern=on,disable-legacy=off \
            -cpu max \
            -forkdaemon ipaddr=\"127.0.0.1\",port=9190 \
            -forkable path=\"/home/dyz/imgs\" \
            -forkgroup gid=0,pid=1";

    // char request[512];
    // int len = sprintf(request, "qemu-system-x86_64 -kernel \"./build/app-test-fork_qemu-x86_64\" -cpu host -enable-kvm -nographic -m 1G -forkable path=\"/images/\" -forkgroup gid=%d,pid=%d", atoi(argv[1]), atoi(argv[2]));

    int len = strlen(request);
    int windex = 0;
    while (windex < len) {
        windex += write(serv_sock, request + windex, len - windex);
    }
    
    int buffer_size = 1024;
    char receive[buffer_size];
    int rindex = 0;
    memset(receive, 0, buffer_size);

    while (1) {
        int str_len = read(serv_sock, receive+rindex, buffer_size - rindex);
        if (str_len == 0) {
            printf("Receive finished\n");
            // close(serv_sock);
            break;
        } else {
            rindex += str_len;
            if (rindex >= buffer_size) {
                printf("Receive buffer is full\n");
                break;
            }
        }
    }

    if (receive[0] >= '0' && receive[0] <= '9') {
        char* p = strtok(receive, " ");
        int gid = atoi(p);
        int pid = atoi(strtok(NULL, " "));
        printf("gid : %d, pid : %d.\n", gid, pid);
    } else {
        // print error 
    }

    // after qum_fork
    close(serv_sock);

    // write(fileno(stdout), receive, rindex);
    // putchar('\n');

    // getchar();
};