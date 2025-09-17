#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    if (argc != 1)
    {
        fprintf(2, "uptime: no arguments expected\n");
        exit(1);
    }

    int ticks = uptime();

    int seconds = ticks / 10;
    int minutes = seconds / 60;
    int hours = minutes / 60;

    seconds = seconds % 60;
    minutes = minutes % 60;

    printf("up %d:%d:%d\n", hours, minutes, seconds); // xv6-printf only supports %d

    exit(0);
}
