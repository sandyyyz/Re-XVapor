#include "types.h"
#include "user.h"

char path[100] = "ls";
int main() {
    char *argv[3];
    int cid;
    int status;
    argv[0] = path;
    argv[1] = 0;
    argv[2] = 0;
    for(int i = 0; i < 100; i++) {
        cid = fork();
        if (cid == 0) {
            exec(path, argv);
        }
    }
    for(int i = 0; i < 100; i++) {
        wait(&status);
        printf("child %d exited with status %d\n", cid, status);
    }
    printf("all child exited\n");
    exit(0);
}