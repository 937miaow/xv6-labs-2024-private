// Shell.

#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"
#include "kernel/stat.h"

// Parsed command representation
#define EXEC 1
#define REDIR 2
#define PIPE 3
#define LIST 4
#define BACK 5

#define MAXARGS 10
#define MAX_BG_JOBS 10 // Maximum number of background jobs

int bg_count = 0; // Number of background jobs

char *available_commands[] = {
    "cat", "cd", "echo", "forktest", "grep", "init", "kill", "ln",
    "ls", "mkdir", "rm", "rmdir", "sh", "stressfs", "usertests",
    "wc", "zombie", "wait", "exit", "pwd", "sleep", "uptime"};
#define NUM_COMMANDS (sizeof(available_commands) / sizeof(available_commands[0]))

void tab_completion(char *, int *, int);

struct cmd
{
  int type;
};

struct execcmd
{
  int type;
  char *argv[MAXARGS];
  char *eargv[MAXARGS];
};

struct redircmd
{
  int type;
  struct cmd *cmd;
  char *file;
  char *efile;
  int mode;
  int fd;
};

struct pipecmd
{
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct listcmd
{
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct backcmd
{
  int type;
  struct cmd *cmd;
};

int fork1(void); // Fork but panics on failure.
void panic(char *);
struct cmd *parsecmd(char *);
void runcmd(struct cmd *) __attribute__((noreturn));

// Execute cmd.  Never returns.
void runcmd(struct cmd *cmd)
{
  int p[2];
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if (cmd == 0)
    exit(1);

  switch (cmd->type)
  {
  default:
    panic("runcmd");

  case EXEC:
    ecmd = (struct execcmd *)cmd;
    if (ecmd->argv[0] == 0)
      exit(1);
    exec(ecmd->argv[0], ecmd->argv);
    fprintf(2, "exec %s failed\n", ecmd->argv[0]);
    break;

  case REDIR:
    rcmd = (struct redircmd *)cmd;
    close(rcmd->fd);
    if (open(rcmd->file, rcmd->mode) < 0)
    {
      fprintf(2, "open %s failed\n", rcmd->file);
      exit(1);
    }
    runcmd(rcmd->cmd);
    break;

  case LIST:
    lcmd = (struct listcmd *)cmd;
    if (fork1() == 0)
      runcmd(lcmd->left);
    wait(0);
    runcmd(lcmd->right);
    break;

  case PIPE:
    pcmd = (struct pipecmd *)cmd;
    if (pipe(p) < 0)
      panic("pipe");
    if (fork1() == 0)
    {
      close(1);
      dup(p[1]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->left);
    }
    if (fork1() == 0)
    {
      close(0);
      dup(p[0]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->right);
    }
    close(p[0]);
    close(p[1]);
    wait(0);
    wait(0);
    break;

  case BACK:
    bcmd = (struct backcmd *)cmd;
    if (fork1() == 0)
      runcmd(bcmd->cmd);
    break;
  }
  exit(0);
}

int getcmd(char *buf, int nbuf)
{
  // check that stdin is a terminal
  struct stat st;
  if (fstat(0, &st) >= 0 && st.type == T_DEVICE && st.dev == 1)
  {
    write(2, "hhh937@xv6$ ", 12);
  }

  memset(buf, 0, nbuf);

  int i = 0;
  char c;
  while (i < nbuf - 1)
  {
    if (read(0, &c, 1) != 1)
    {
      break;
    }

    // fprintf(2, "[DEBUG: char_code=%d]\n", c);

    switch (c)
    {
    case '\n':
    case '\r':
      // handle <newline>/<return> characters
      buf[i] = '\0';
      write(2, "\n", 1); // ensure newline after command
      return 0;

    case '\t':
      // handle <tab> input
      tab_completion(buf, &i, nbuf);
      break; // break switch, continue while loop

    case '\b':
    case 127:
      // handle <backspace>
      if (i > 0)
      {
        i--;
        write(2, "\b \b", 3); // erase character from terminal
      }
      break;

    default:
      // other control characters
      if (c >= ' ' && c <= '~')
      {
        buf[i++] = c;
        write(2, &c, 1); // echo character
      }
      break;
    }
  }

  if (i == 0) // EOF or error
    return -1;

  // buffer full
  if (i == nbuf - 1)
  {
    buf[i] = '\0';
  }

  return 0;
}

int main(void)
{
  static char buf[100];
  int fd;
  int background = 0;

  // Ensure that three file descriptors are open.
  while ((fd = open("console", O_RDWR)) >= 0)
  {
    if (fd >= 3)
    {
      close(fd);
      break;
    }
  }

  // Read and run input commands.
  while (getcmd(buf, sizeof(buf)) >= 0)
  {
    background = 0;

    // check for '&' indicating background job
    int len = strlen(buf);
    if (len > 0 && buf[len - 2] == '&') // '&' is before '\0'
    {
      background = 1;
      buf[len - 2] = '\0'; // Remove '&' from command
    }

    // built-in command: cd
    if (buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' ')
    {
      // Chdir must be called by the parent, not the child.
      buf[strlen(buf) - 1] = 0; // chop \n
      if (chdir(buf + 3) < 0)
        fprintf(2, "cannot cd %s\n", buf + 3);
      continue;
    }

    // built-in command: wait
    if (strcmp(buf, "wait") == 0 || strcmp(buf, "wait\n") == 0)
    {
      while (wait(0) >= 0)
        ;
      bg_count = 0; // Reset background job count after waiting
      continue;
    }

    int pid = fork1();
    if (pid == 0)
      runcmd(parsecmd(buf));

    if (!background)
    {
      wait(0); // Wait for foreground job to finish
    }
    else
    {
      // Background job: store its PID
      if (bg_count < MAX_BG_JOBS)
      {
        bg_count++;
        printf("[%d] %d\n", bg_count, pid);
      }
    }
  }

  exit(0);
}

void panic(char *s)
{
  fprintf(2, "%s\n", s);
  exit(1);
}

int fork1(void)
{
  int pid;

  pid = fork();
  if (pid == -1)
    panic("fork");
  return pid;
}

// PAGEBREAK!
//  Constructors

struct cmd *
execcmd(void)
{
  struct execcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = EXEC;
  return (struct cmd *)cmd;
}

struct cmd *
redircmd(struct cmd *subcmd, char *file, char *efile, int mode, int fd)
{
  struct redircmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = REDIR;
  cmd->cmd = subcmd;
  cmd->file = file;
  cmd->efile = efile;
  cmd->mode = mode;
  cmd->fd = fd;
  return (struct cmd *)cmd;
}

struct cmd *
pipecmd(struct cmd *left, struct cmd *right)
{
  struct pipecmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = PIPE;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd *)cmd;
}

struct cmd *
listcmd(struct cmd *left, struct cmd *right)
{
  struct listcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = LIST;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd *)cmd;
}

struct cmd *
backcmd(struct cmd *subcmd)
{
  struct backcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = BACK;
  cmd->cmd = subcmd;
  return (struct cmd *)cmd;
}
// PAGEBREAK!
//  Parsing

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>&;()";

int gettoken(char **ps, char *es, char **q, char **eq)
{
  char *s;
  int ret;

  s = *ps;
  while (s < es && strchr(whitespace, *s))
    s++;
  if (q)
    *q = s;
  ret = *s;
  switch (*s)
  {
  case 0:
    break;
  case '|':
  case '(':
  case ')':
  case ';':
  case '&':
  case '<':
    s++;
    break;
  case '>':
    s++;
    if (*s == '>')
    {
      ret = '+';
      s++;
    }
    break;
  default:
    ret = 'a';
    while (s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
      s++;
    break;
  }
  if (eq)
    *eq = s;

  while (s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return ret;
}

int peek(char **ps, char *es, char *toks)
{
  char *s;

  s = *ps;
  while (s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return *s && strchr(toks, *s);
}

struct cmd *parseline(char **, char *);
struct cmd *parsepipe(char **, char *);
struct cmd *parseexec(char **, char *);
struct cmd *nulterminate(struct cmd *);

struct cmd *
parsecmd(char *s)
{
  char *es;
  struct cmd *cmd;

  es = s + strlen(s);
  cmd = parseline(&s, es);
  peek(&s, es, "");
  if (s != es)
  {
    fprintf(2, "leftovers: %s\n", s);
    panic("syntax");
  }
  nulterminate(cmd);
  return cmd;
}

struct cmd *
parseline(char **ps, char *es)
{
  struct cmd *cmd;

  cmd = parsepipe(ps, es);
  while (peek(ps, es, "&"))
  {
    gettoken(ps, es, 0, 0);
    cmd = backcmd(cmd);
  }
  if (peek(ps, es, ";"))
  {
    gettoken(ps, es, 0, 0);
    cmd = listcmd(cmd, parseline(ps, es));
  }
  return cmd;
}

struct cmd *
parsepipe(char **ps, char *es)
{
  struct cmd *cmd;

  cmd = parseexec(ps, es);
  if (peek(ps, es, "|"))
  {
    gettoken(ps, es, 0, 0);
    cmd = pipecmd(cmd, parsepipe(ps, es));
  }
  return cmd;
}

struct cmd *
parseredirs(struct cmd *cmd, char **ps, char *es)
{
  int tok;
  char *q, *eq;

  while (peek(ps, es, "<>"))
  {
    tok = gettoken(ps, es, 0, 0);
    if (gettoken(ps, es, &q, &eq) != 'a')
      panic("missing file for redirection");
    switch (tok)
    {
    case '<':
      cmd = redircmd(cmd, q, eq, O_RDONLY, 0);
      break;
    case '>':
      cmd = redircmd(cmd, q, eq, O_WRONLY | O_CREATE | O_TRUNC, 1);
      break;
    case '+': // >>
      cmd = redircmd(cmd, q, eq, O_WRONLY | O_CREATE, 1);
      break;
    }
  }
  return cmd;
}

struct cmd *
parseblock(char **ps, char *es)
{
  struct cmd *cmd;

  if (!peek(ps, es, "("))
    panic("parseblock");
  gettoken(ps, es, 0, 0);
  cmd = parseline(ps, es);
  if (!peek(ps, es, ")"))
    panic("syntax - missing )");
  gettoken(ps, es, 0, 0);
  cmd = parseredirs(cmd, ps, es);
  return cmd;
}

struct cmd *
parseexec(char **ps, char *es)
{
  char *q, *eq;
  int tok, argc;
  struct execcmd *cmd;
  struct cmd *ret;

  if (peek(ps, es, "("))
    return parseblock(ps, es);

  ret = execcmd();
  cmd = (struct execcmd *)ret;

  argc = 0;
  ret = parseredirs(ret, ps, es);
  while (!peek(ps, es, "|)&;"))
  {
    if ((tok = gettoken(ps, es, &q, &eq)) == 0)
      break;
    if (tok != 'a')
      panic("syntax");
    cmd->argv[argc] = q;
    cmd->eargv[argc] = eq;
    argc++;
    if (argc >= MAXARGS)
      panic("too many args");
    ret = parseredirs(ret, ps, es);
  }
  cmd->argv[argc] = 0;
  cmd->eargv[argc] = 0;
  return ret;
}

// NUL-terminate all the counted strings.
struct cmd *
nulterminate(struct cmd *cmd)
{
  int i;
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if (cmd == 0)
    return 0;

  switch (cmd->type)
  {
  case EXEC:
    ecmd = (struct execcmd *)cmd;
    for (i = 0; ecmd->argv[i]; i++)
      *ecmd->eargv[i] = 0;
    break;

  case REDIR:
    rcmd = (struct redircmd *)cmd;
    nulterminate(rcmd->cmd);
    *rcmd->efile = 0;
    break;

  case PIPE:
    pcmd = (struct pipecmd *)cmd;
    nulterminate(pcmd->left);
    nulterminate(pcmd->right);
    break;

  case LIST:
    lcmd = (struct listcmd *)cmd;
    nulterminate(lcmd->left);
    nulterminate(lcmd->right);
    break;

  case BACK:
    bcmd = (struct backcmd *)cmd;
    nulterminate(bcmd->cmd);
    break;
  }
  return cmd;
}

void tab_completion(char *buf, int *index, int nbuf)
{
  char current_word[100];
  int word_start = 0;
  int word_len = 0;

  // Find the start of the current word
  for (int i = *index - 1; i >= 0; i--)
  {
    if (buf[i] == ' ' || i == 0)
    {
      word_start = (buf[i] == ' ') ? i + 1 : i;
      break;
    }
  }
  word_len = *index - word_start;

  // if no current word, list all commands
  if (word_len == 0)
  {
    write(2, "\n", 1);
    for (int i = 0; i < NUM_COMMANDS; i++)
    {
      fprintf(2, "%s  ", available_commands[i]);
      if ((i + 1) % 5 == 0)
        write(2, "\n", 1); // New line after every 5 commands
    }
    write(2, "\n$ ", 3);
    write(2, buf, *index);
    return;
  }

  if (word_len >= sizeof(current_word) - 1)
    return;

  // Copy the current word
  strncpy(current_word, buf + word_start, word_len);
  current_word[word_len] = '\0';

  // match current word with available commands
  char *matches[100];
  int match_cnt = 0;

  for (int i = 0; i < NUM_COMMANDS; i++)
  {
    if (strncmp(available_commands[i], current_word, word_len) == 0)
    {
      matches[match_cnt++] = available_commands[i];
    }
  }

  if (match_cnt == 0)
  {
    return;
  }
  else if (match_cnt == 1) // Single match: complete the command
  {
    char *match = matches[0];
    int chars_to_add = strlen(match) - word_len;

    if (*index + chars_to_add + 1 >= nbuf)
    {
      return;
    }

    // 1. Update the input buffer
    strncat(buf, match + word_len, chars_to_add);
    *index += chars_to_add;
    buf[*index] = ' ';
    (*index)++;
    buf[*index] = '\0';

    // 2. Update the terminal display
    for (int i = 0; i < word_len; ++i)
    {
      write(2, "\b \b", 3);
    }

    write(2, match, strlen(match));
    write(2, " ", 1);
  }
  else // Multiple matches: list them
  {
    write(2, "\n", 1);
    for (int i = 0; i < match_cnt; i++)
    {
      fprintf(2, "%s  ", matches[i]);
      if ((i + 1) % 6 == 0)
        write(2, "\n", 1);
    }
    write(2, "\n$ ", 3);
    write(2, buf, *index);
  }
}
