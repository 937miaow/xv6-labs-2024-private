#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void sieve(int read_fd);

int main(int argc, char *argv[])
{
    if (strcmp(argv[0], "-h") == 0 || strcmp(argv[0], "--help") == 0)
    {
        printf("Usage: primes [number]\n"
               "default is 280, but the max is 283,\n"
               "due to the res of xv6\n");
        exit(0);
    }

    int n;
    if (argc == 1) // Default to 280 if no argument is provided
    {
        n = 280;
    }
    else if (argc == 2) // One argument provided
    {
        n = atoi(argv[1]);
        if (n < 2)
        {
            printf("Usage: primes [number]\n"
                   "default is 280, but the max is 283,\n"
                   "due to the res of xv6\n");
            exit(1);
        }
    }
    else
    {
        printf("Usage: primes [number]\n"
               "default is 280, but the max is 283,\n"
               "due to the res of xv6\n");
        exit(1);
    }

    int p[2];
    if (pipe(p) < 0)
        exit(1);

    if (fork() == 0)
    {
        // Child process - start the sieve
        close(p[1]); // Close write end
        sieve(p[0]);
        close(p[0]);
        exit(0);
    }
    else
    {
        // Parent process - generate numbers
        close(p[0]); // Close read end
        for (int i = 2; i <= n; i++)
        {
            write(p[1], &i, sizeof(int));
        }
        close(p[1]); // Close write end to signal EOF

        wait(0); // Wait for all child processes
        exit(0);
    }
}

void sieve(int read_fd)
{
    int prime;
    int num;

    // Read the first number from the input
    if (read(read_fd, &prime, sizeof(int)) <= 0)
    {
        close(read_fd);
        return;
    }

    printf("prime %d\n", prime);

    // Check if there are more numbers to process
    if (read(read_fd, &num, sizeof(int)) <= 0)
    {
        close(read_fd);
        return; // Only one number
    }

    int p[2];
    if (pipe(p) < 0)
        exit(1);

    if (fork() == 0)
    {
        // Child process - continue the sieve
        close(p[1]);    // Close write end
        close(read_fd); // Close original read end
        sieve(p[0]);
        close(p[0]);
        exit(0);
    }
    else
    {
        // Parent process - filter multiples
        close(p[0]); // Close read end

        // Write the first number we read (if it's not a multiple)
        if (num % prime != 0)
        {
            write(p[1], &num, sizeof(int));
        }

        // Continue reading and filtering
        while (read(read_fd, &num, sizeof(num)) > 0)
        {
            if (num % prime != 0)
            {
                write(p[1], &num, sizeof(int));
            }
        }

        close(read_fd); // Close original read end
        close(p[1]);    // Close write end to signal EOF

        wait(0); // Wait for child to finish
    }
}
