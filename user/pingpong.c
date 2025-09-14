#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int ptoc[2]; // pipe from parent to child
    int ctop[2]; // pipe from child to parent
    char buf[5];

    pipe(ptoc);
    pipe(ctop);

    if (fork() == 0)
    {
        close(ptoc[1]); // close write end of ptoc
        close(ctop[0]); // close read end of ctop

        read(ptoc[0], buf, 4);
        buf[4] = '\0';

        int pid = getpid();
        printf("%d: received %s\n", pid, buf);

        write(ctop[1], "pong", 4);

        close(ptoc[0]);
        close(ctop[1]);
        exit(0);
    }
    else
    {
        close(ptoc[0]); // close read end of ptoc
        close(ctop[1]); // close write end of ctop

        int pid = getpid();
        write(ptoc[1], "ping", 4);

        read(ctop[0], buf, 4);
        buf[4] = '\0';
        printf("%d: received %s\n", pid, buf);

        wait(0);

        close(ptoc[1]);
        close(ctop[0]);
        exit(0);
    }
}
