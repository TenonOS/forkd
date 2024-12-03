#ifndef PACKAGE_H
#define PACKAGE_H

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

char* serialize_package(package *p, int* str_len);
package* deserialize_package(char *buf);
void check_package(package *p);
// void set_package(package *p, int command, int len, char *data);
// void extract_package(package *p, int command, int len, char *data);

#endif