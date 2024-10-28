#include "forkd.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <linux/types.h>


int main() {

    pid_t pid;
    pid = vfork();
    if(pid < 0){

    } else {

    }
}