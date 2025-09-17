#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(2, "Usage: xargs command [arguments...]\n");
        exit(1);
    }

    char *cmd = argv[1];
    char *cmd_argv[MAXARG];
    int i;

    // copy base arguments
    for (i = 1; i < argc && i < MAXARG; i++)
    {
        cmd_argv[i - 1] = argv[i];
    }
    int base_argc = i - 1;

    char line[512];
    char *args[MAXARG];

    while (1)
    {
        // read a line from stdin
        int pos = 0;
        char c;
        while (read(0, &c, 1) > 0)
        {
            if (c == '\n')
                break;
            if (pos < sizeof(line) - 1)
            {
                line[pos++] = c;
            }
        }

        if (pos == 0)
            break; // EOF or read error

        line[pos] = '\0';

        int cmd_argc = base_argc;
        for (i = 0; i < base_argc; i++)
        {
            args[i] = cmd_argv[i];
        }

        char *p = line;
        while (*p && cmd_argc < MAXARG - 1)
        {
            // skip spaces
            while (*p == ' ')
                p++;
            if (!*p)
                break;

            // find the start and theend of the argument
            args[cmd_argc++] = p;
            while (*p && *p != ' ')
                p++;
            if (*p)
                *p++ = '\0';
        }
        args[cmd_argc] = 0;

        // execute the command
        if (fork() == 0)
        {
            exec(cmd, args);

            // if exec fails
            fprintf(2, "xargs: exec %s failed\n", cmd);
            exit(1);
        }
        wait(0);
    }

    exit(0);
}
