#include "kernel/types.h"
#include "user.h"

#define BUF_SIZE 128

int main(int argc, char *argv[])
{
    int ppfd[2], cpfd[2], prfd, pwfd, crfd, cwfd, num;
    char buffer[BUF_SIZE];

    // create pipe
    if (pipe(ppfd) == -1 || pipe(cpfd) == -1)
    {
        printf("pipe fail\n");
        exit(-1);
    }
    prfd = ppfd[0];
    pwfd = cpfd[1];
    crfd = cpfd[0];
    cwfd = ppfd[1];

    // create child process
    switch (fork())
    {
    case -1:
        printf("fork fail\n");
        exit(-1);
    case 0:     // child
        if (close(prfd) == -1 || close(pwfd) == -1)
        {
            printf("child close fail\n");
            exit(-1);
        }
        if ((num = read(crfd, buffer, BUF_SIZE)) == -1)
        {
            printf("child read fail\n");
            exit(-1);
        }
        buffer[num] = '\0';
        printf("%d: received %s\n", getpid(), buffer);
        if (write(cwfd, "pong", 4) == -1)
        {
            printf("parent write fail\n");
            exit(-1);
        }
        break;
    default:    // parent
        if (close(crfd) == -1 || close(cwfd) == -1)
        {
            printf("parent close fail\n");
            exit(-1);
        }
        if (write(pwfd, "ping", 4) == -1)
        {
            printf("parent write fail\n");
            exit(-1);
        }
        if ((num = read(prfd, buffer, BUF_SIZE)) == -1)
        {
            printf("parent read fail\n");
            exit(-1);
        }
        buffer[num] = '\0';
        printf("%d: received %s\n", getpid(), buffer);
        break;
    }

    exit(0);
}