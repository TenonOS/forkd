#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define BUF_SIZE 256

enum CMD {
    CMD_ERROR = 0,
    CMD_INIT,
    CMD_INITRET,
    CMD_FORKPARA,
    CMD_QEMUFORK,
    CMD_CLIENTCOLSE,
    CMD_WAITPID,
    CMD_WAITPIDRET,
    CMD_SENDGIDPID,
    CMD_KILL,
};

typedef struct {
    enum CMD command;
    int len;
    char *data;
} package;

void check_package(package *p) {
    printf("-----------------------------------------\n");
    printf("check package:\n");
    printf("%d %d ", p->command, p->len);
    fflush(stdout);
    write(fileno(stdout), p->data, p->len);
    printf("\n-----------------------------------------\n");
}

static void check_str(char* str, int len) {
    for (int i = 0; i < len; i++) {
        // putchar(str[i]+48);
        printf("%d ", str[i]);
    }
    putchar('\n');
}

static void readlen(int serv_sock, char* buffer, int len) {
    int str_len = 0;
    while (str_len < len) {
        int read_len = read(serv_sock, buffer+str_len, len-str_len);
        str_len += read_len;
    }
    return ;
}

static void receive_package(int serv_sock, package* pack) {
    // package* pack = (package*)malloc(sizeof(package));
    int cmd_len = sizeof(pack->command);
    // 读取 CMD
    readlen(serv_sock, (char*)&pack->command, cmd_len);
    // 读取 DATA_LEN
    int args_len = sizeof(pack->len);
    readlen(serv_sock, (char*)&pack->len, args_len);
    // 读取 DATA
    if (pack->len > 0) {
        pack->data = (char*)malloc(pack->len);
        readlen(serv_sock, pack->data, pack->len);
    }
    return ;
}

static char* serialize_package(package *p, int* str_len) {
    *str_len = sizeof(p->command) + sizeof(p->len) + p->len;
    char* buf = (char*)malloc(*str_len);
    int offset = 0;
    memcpy(buf, &p->command, sizeof(p->command));
    offset += sizeof(p->command);
    memcpy(buf + offset, &p->len, sizeof(p->len));
    if (p->len > 0) {
        offset += sizeof(p->len);
        memcpy(buf + offset, p->data, p->len);
    }
    // check_str(buf, str_len);
    return buf;
}

static void send_package(package* p, int serv_sock) {
    int str_len;
    char* message = serialize_package(p, &str_len);
    // check_str(message, strlen(message));
    // int len = strlen(message);
    int windex = 0;
    while (windex < str_len) {
        windex += write(serv_sock, message + windex, str_len - windex);
    }
    free(message);
    if (p->data!= NULL)
        free(p->data);
    // free(p);
}

int main(int argc, char *argv[]) {
    // qemu init

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

    int gid = 0;
    int pid = 1;

    package* send_pack = (package*)malloc(sizeof(package));
    send_pack->command = CMD_INIT;
    char* temp = (char*)malloc(sizeof(char)*BUF_SIZE);
    // 换成自己的gid和pid
    send_pack->len = sprintf(temp, "%d %d", gid, pid);
    send_pack->data = (char*)malloc(send_pack->len);
    memcpy(send_pack->data, temp, send_pack->len);
    // printf("send_pack->len: %d\n", send_pack->len);
    free(temp);

    send_package(send_pack, serv_sock);
    free(send_pack);

    // 如果自己的 gid 是 0，那么就接收新分配的 gid 并设置
    // 否则不需要额外处理
    if (gid == 0) {
        package* receive_pack = (package*)malloc(sizeof(package));
        receive_package(serv_sock, receive_pack);
        if (receive_pack->command == CMD_INITRET) {
            char* receive = receive_pack->data;
            char* p = strtok(receive, " ");
            int new_gid = atoi(p);
            // handle
            gid = new_gid;
        } else {
            // error handling
        }
    }
    

    // qemu fork

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
            -forkgroup gid=1,pid=1";

    send_pack = (package*)malloc(sizeof(package));
    send_pack->command = CMD_FORKPARA;
    send_pack->len = strlen(request);
    send_pack->data = (char*)malloc(send_pack->len);
    memcpy(send_pack->data, request, send_pack->len);
    send_package(send_pack, serv_sock);

    // gid 在 init 阶段已经保证合理，因此此处进返回子进程的 pid
    package* receive_pack = (package*)malloc(sizeof(package));
    receive_package(serv_sock, receive_pack);
    char* receive = receive_pack->data;
    char* p = strtok(receive, " ");
    int cpid = atoi(p);
    free(receive);
    free(receive_pack);
    // 此处处理子进程的 pid

    // after qem_fork finished

    send_pack->command = CMD_QEMUFORK;
    send_pack->len = 0;
    send_pack->data = NULL;
    send_package(send_pack, serv_sock);
    free(send_pack);

    // qemu close

    send_pack = (package*)malloc(sizeof(package));
    send_pack->command = CMD_CLIENTCOLSE;
    send_pack->len = 0;
    send_pack->data = NULL;
    send_package(send_pack, serv_sock);
    close(serv_sock);
    free(send_pack);

    // qemu waitpid

    send_pack = (package*)malloc(sizeof(package));
    send_pack->command = CMD_WAITPID;
    temp = (char*)malloc(sizeof(char)*BUF_SIZE);
    // 换成要等待的子进程的gid pid
    send_pack->len = sprintf(temp, "%d %d", gid, pid);
    send_pack->data = (char*)malloc(send_pack->len);
    memcpy(send_pack->data, temp, send_pack->len);
    free(temp);
    send_package(send_pack, serv_sock);
    free(send_pack);


    receive_pack = (package*)malloc(sizeof(package));
    // 该函数返回，说明相应进程已经结束，waitpid 直接返回等待的 pid 就行了
    receive_package(serv_sock, receive_pack);
    free(receive_pack);


    // qemu kill

    /*
    send_pack = (package*)malloc(sizeof(package));
    send_pack->command = CMD_KILL;
    temp = (char*)malloc(sizeof(char)*BUF_SIZE);
    // 换成要杀死进程的 gid pid
    send_pack->len = sprintf(temp, "%d %d", gid, pid);
    send_pack->data = (char*)malloc(send_pack->len);
    memcpy(send_pack->data, temp, send_pack->len);
    free(temp);
    send_package(send_pack, serv_sock);
    free(send_pack);
    */
};