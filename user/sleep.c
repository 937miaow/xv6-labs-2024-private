#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    if (argc < 2) // lack of ticks
    {
        fprintf(2, "sleep: lack of ticks\n"
                   "please use 'sleep --help' for more information\n");
        exit(1);
    }

    if (strcmp(argv[1], "--help") == 0) // help message
    {
        fprintf(1, "Usage: sleep ticks\n"
                   "Pause execution for a specified number of clock ticks.\n"
                   "Example: sleep 100 (sleeps for 100 clock ticks)\n");
        exit(1);
    }

    for (char *p = argv[1]; *p; p++) // check if argv[1] is a valid number
    {
        if (*p < '0' || *p > '9')
        {
            fprintf(2, "sleep: invalid number of ticks\n");
            exit(1);
        }
    }

    int ticks = atoi(argv[1]);

    if (ticks < 0) // negative ticks
    {
        fprintf(2, "sleep: invalid number of ticks\n");
        exit(1);
    }

    sleep(ticks);
    exit(0);
}
