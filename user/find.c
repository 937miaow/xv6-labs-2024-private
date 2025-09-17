#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "user/match.c"

void find(char *path, char *filename);

char *get_basename(char *path)
{
    static char buf[DIRSIZ + 1];
    char *p;

    // find first character after last slash.
    for (p = path + strlen(path); p >= path && *p != '/'; p--)
        ;
    p++;

    // copy the basename into buf
    int len = 0;
    while (len < DIRSIZ && p[len] != '\0')
    {
        buf[len] = p[len];
        len++;
    }
    buf[len] = '\0';

    return buf;
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(2, "Usage: find <path> <filename>\n");
        exit(1);
    }
    find(argv[1], argv[2]);
    exit(0);
}

void find(char *path, char *filename)
{
    char buf[512], *p;
    int fd;
    struct stat st;
    struct dirent de;

    if ((fd = open(path, O_RDONLY)) < 0)
    {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    if (fstat(fd, &st) < 0)
    {
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    switch (st.type)
    {
    case T_FILE:
        char *basename = get_basename(path);

        // check if filename contains regex characters
        int is_regex = 0;
        for (char *p = filename; *p != '\0'; p++)
        {
            if (*p == '^' || *p == '$' || *p == '.' || *p == '*')
            {
                is_regex = 1;
                break;
            }
        }

        int found;
        if (is_regex)
        {
            found = match(filename, basename);
        }
        else
        {
            found = (strcmp(filename, basename) == 0);
        }

        if (found)
        {
            printf("%s\n", path);
        }
        break;
    case T_DIR:
        if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf)
        {
            printf("find: path too long\n");
            break;
        }
        strcpy(buf, path);
        p = buf + strlen(buf);
        *p++ = '/';

        while (read(fd, &de, sizeof(de)) == sizeof(de))
        {
            if (de.inum == 0)
                continue;
            if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
                continue;

            int namelen = strlen(de.name);
            if (namelen > DIRSIZ)
                namelen = DIRSIZ;
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;

            find(buf, filename);
        }
    }

    close(fd);
}
