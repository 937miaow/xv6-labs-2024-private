#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    // Clear the terminal screen using ANSI escape codes
    write(1, "\033[2J\033[3J\033[H", 11);
    return 0;
}