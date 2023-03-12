/* $begin shellmain */
// https://2fwww.dandelioncloud.cn/article/details/1530113346808594433
// https://github.com/zhangyi1357/CSAPP-Labs/blob/main/shlab-handout/tsh.c
#include "csapp.h"
#include "myjob.h"
#define MAXARGS 128
struct cmd {
  char *argv[MAXARGS];
  char *in, *out;
};

/* Function prototypes */
void eval(char *cmdline);
int parse(struct cmd *cmd, char *buf, int cmdnum);
int builtin_command(char **argv);
void do_bgfg(char **argv);
void sigchld_handler(int sig);
void sigquit_handler(int sig);
void sigstop_handler(int sig);
void waitfg();
extern struct job_t myjobs[MAXJOBS];  // myjobs数组

int main() {
  char cmdline[MAXLINE];            /* Command line */
  Signal(SIGINT, sigquit_handler);  /* ctrl-c */
  Signal(SIGTSTP, sigstop_handler); /* ctrl-z */
  Signal(SIGCHLD, sigchld_handler); /* Terminated or stopped child */
  initjobs(myjobs);

  while (1) {
    /* Read */
    printf("dsh> ");
    Fgets(cmdline, MAXLINE, stdin);
    if (feof(stdin)) exit(0);

    /* Evaluate */
    eval(cmdline);
  }
}
/* $end shellmain */

/* $begin eval */
/* eval - Evaluate a command line */
void eval(char *cmdline) {
  char *argv[MAXARGS]; /* Argument list execve() */
  char buf[MAXLINE];   /* Holds modified command line */
  int bg;              /* Should the job run in bg or fg? */
  pid_t pid;           /* Process id */
  struct cmd cmds[1024];
  int pfd[1024][2];
  sigset_t mask_all, mask_chld, prev_chld;
  Sigfillset(&mask_all);
  Sigemptyset(&mask_chld);
  Sigaddset(&mask_chld, SIGCHLD);

  strcpy(buf, cmdline);
  int res = 0;
  int cmd_num = 0;
  char *curcmd = buf;

  if (buf[strlen(buf) - 1] == '\n') buf[strlen(buf) - 1] = '\0';
  cmd_num = 0;
  char *nextcmd = buf;

  while ((curcmd = strsep(&nextcmd, "|"))) {
    if (parse(cmds, curcmd, cmd_num++) < 0) {
      cmd_num--;
      break;
    }

    if (cmd_num == MAXPIPE + 1) break;
  }

  if (!cmd_num) return;

  int pipe_num = cmd_num - 1;  // 根据命令数确定要创建的管道数目

  for (int i = 0; i < pipe_num; i++) {  // 创建管道
    if (pipe(pfd[i])) {
      perror("pipe");
      exit(0);
    }
  }
  int i;
  for (i = 0; i < cmd_num; i++) {  // 管道数目决定创建子进程个数
    if ((pid = fork()) == 0) break;
  }

  if (pid == 0) {
    if (pipe_num) {  // 用户输入的命令中含有管道

      if (i == 0) {  // 第一个创建的子进程
        dup2(pfd[0][1], STDOUT_FILENO);
        close(pfd[0][1]);
        close(pfd[0][0]);

        for (
            int j = 1; j < pipe_num;
            j++) {  // 在该子进程执行期间,关闭该进程使用不到的其他管道的读端和写端
          close(pfd[j][0]);
          close(pfd[j][1]);
        }

      } else if (i == pipe_num) {  // 最后一个创建的子进程
        dup2(pfd[i - 1][0], STDIN_FILENO);
        close(pfd[i - 1][0]);
        close(pfd[i - 1][1]);

        for (int j = 0; j < pipe_num - 1;
             j++) {  // 在该子进程执行期间,关闭该进程不使用的其他管道的读/写端
          close(pfd[j][0]);
          close(pfd[j][1]);
        }

      } else {
        dup2(pfd[i - 1][0], STDIN_FILENO);  // 重定中间进程的标准输入至管道读端
        close(pfd[i - 1][0]);
        close(pfd[i - 1][1]);  // close管道写端

        dup2(pfd[i][1], STDOUT_FILENO);  // 重定中间进程的标准输出至管道写端
        close(pfd[i][1]);
        close(pfd[i][0]);  // close管道读端

        for (int j = 0; j < pipe_num; j++)  // 关闭不使用的管道读写两端
          if (j != i || j != i - 1) {
            close(pfd[j][0]);
            close(pfd[j][1]);
          }
      }
    }
    if (cmds[i].in) { /*用户在命令中使用了输入重定向*/
      int fd = open(cmds[i].in, O_RDONLY);  // 打开用户指定的重定向文件,只读即可
      if (fd != -1) dup2(fd, STDIN_FILENO);  // 将标准输入重定向给该文件
    }
    if (cmds[i].out) { /*用户在命令中使用了输出重定向*/
      int fd = open(cmds[i].out, O_WRONLY | O_CREAT | O_TRUNC,
                    0644);  // 使用写权限打开用户指定的重定向文件
      if (fd != -1) dup2(fd, STDOUT_FILENO);  // 将标准输出重定向给该文件
    }

    execvp(cmds[i].argv[0], cmds[i].argv);  // 执行用户输入的命令
    fprintf(stderr, "executing %s error.\n", cmds[i].argv[0]);
    exit(0);
  }

  /*  parent */
  for (i = 0; i < pipe_num; i++) { /*父进程不参与命令执行,关闭其掌握的管道两端*/
    close(pfd[i][0]);
    close(pfd[i][1]);
  }
  for (int i = 0; i < cmd_num; i++) { /*循环回首子进程*/
    wait(NULL);
  }
  return;
}

/* If first arg is a builtin command, run it and return true */
int builtin_command(char **argv) {
  if (!strcmp(argv[0], "quit")) /* quit command */
    exit(0);
  if (!strcmp(argv[0], "&")) /* Ignore singleton & */
    return 1;
  else if (!strcmp(argv[0], "jobs")) {
    listjobs(myjobs);
    return 1;
  } else if (!strcmp(argv[0], "bg")) { /*bg command*/
    do_bgfg(argv);
    return 1;
  } else if (!strcmp(argv[0], "fg")) { /*fg command*/
    do_bgfg(argv);
    return 1;
  }
  return 0; /* Not a builtin command */
}
/* $end eval */

int parse(struct cmd *cmd, char *buf, int cmdnum) {
  int n = 0;
  char *p = buf;
  cmd[cmdnum].in = cmd[cmdnum].out = NULL;

  // ls -l -d -a -F  > out
  while (*p != '\0') {
    if (*p == ' ') {  // 将字符串中所有的空格,替换成'\0',方便后续拆分字符串
      *p++ = '\0';
      continue;
    }

    if (*p == '<') {
      *p = '\0';
      while (*(++p) == ' ')
        ; /* cat <     file 处理连续多个空格的情况*/
      cmd[cmdnum].in = p;
      if (*p++ == '\0')  // 输入重定向<后面没有文件名
        return -1;
      continue;
    }

    if (*p == '>') {
      *p = '\0';
      while (*(++p) == ' ')
        ;
      cmd[cmdnum].out = p;
      if (*p++ == '\0') return -1;
      continue;
    }

    if (*p != ' ' && ((p == buf) || *(p - 1) == '\0')) {
      if (n < 1024) {
        cmd[cmdnum].argv[n++] = p++;  //"ls -l -R > file"
        continue;
      } else {
        return -1;
      }
    }
    p++;
  }

  if (n == 0) {
    return -1;
  }

  cmd[cmdnum].argv[n] = NULL;

  return 0;
}

void do_bgfg(char **argv) {
  int number = 0;           /* used to store the converted number */
  char *ptr = argv[1];      /* get the pointer to argument 1 */
  char *endptr = NULL;      /* used for error handling */
  struct job_t *job = NULL; /* used to store job pointer */
  if (!argv[1]) {           /* returns if missing argument */
    printf("%s command requires PID or %%jobid argument\n", argv[0]);
    return;
  }

  sigset_t mask_all, prev_all;
  Sigfillset(&mask_all);
  Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
  if (*ptr == '%') { /* if argument 1 is job ID */
    ptr++;           /* adjust pointer */
    number = strtol(ptr, &endptr, 10);
    if (ptr == endptr) { /* handles convert error */
      printf("%s: argument must be a PID or %%jobid\n", argv[0]);
      Sigprocmask(SIG_SETMASK, &prev_all, NULL);
      return;
    }
    job = getjobjid(myjobs, number); /* get the job */
    if (!job) {                      /* handles if there is no such job */
      printf("%%%d: No such job\n", number);
      Sigprocmask(SIG_SETMASK, &prev_all, NULL);
      return;
    }
  } else { /* if argument 1 is pid */
    number = strtol(ptr, &endptr, 10);
    if (ptr == endptr) { /* handles convert error */
      printf("%s: argument must be a PID or %%jobid\n", argv[0]);
      Sigprocmask(SIG_SETMASK, &prev_all, NULL);
      return;
    }
    job = getjobpid(myjobs, (pid_t)number); /* get the job */
    if (!job) { /* handles if there is no such job */
      printf("(%d): No such process\n", number);
      Sigprocmask(SIG_SETMASK, &prev_all, NULL);
      return;
    }
  }
  /* change process state and update the job list*/
  if (!strcmp(argv[0], "bg")) { /*bg: turns ST to BG, send SIGCONT */
    if (job->state == ST) {
      Kill(-(job->pid[0]), SIGCONT);
      printf("[%d] (%d) %s", job->jid, job->pid[0], job->cmdline);
      job->state = BG;
      Sigprocmask(SIG_SETMASK, &prev_all, NULL);
    }
  } else if (!strcmp(argv[0], "fg")) { /* fg: turns ST/BG to FG, sent SIGCONT */
    if (job->state == ST) {
      Kill(-(job->pid[0]), SIGCONT);
      job->state = BG;
      Sigprocmask(SIG_SETMASK, &prev_all, NULL);
    }
    if (job->state == BG) {
      job->state = FG;
      Sigprocmask(SIG_SETMASK, &prev_all, NULL);
      waitfg();
    }
  }
  return;
}

void waitfg() {
  sigset_t mask_all, prev_all;
  Sigfillset(&mask_all);
  Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
  while (fgpid(myjobs)) { /* handles all zombie processes in signal handler */
    Sigprocmask(SIG_SETMASK, &prev_all, NULL);
    sleep(0.01); /* busy loop only */
  }
  return;
}

void sigchld_handler(int sig) {
  int olderrno = errno; /* store old errno */
  int status;           /* used to trace pid's status */
  sigset_t mask_all, prev_all;
  pid_t pid;

  Sigfillset(&mask_all);
  while ((pid = Waitpid(-1, &status, WUNTRACED | WNOHANG)) >
         0) { /* waitpid without waiting(WNOHANG) */
    Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
    if (WIFSIGNALED(status) &&
        WTERMSIG(status) ==
            SIGINT) { /* it the terminate signal hasn't been catched */
      struct job_t *job = getjobpid(myjobs, pid);
      if (job) {
        printf("Job [%d] (%d) terminated by signal %d\n", pid2jid(pid), pid,
               WTERMSIG(status));
        /* handles here */
      }
      Sigprocmask(SIG_SETMASK, &prev_all, NULL);
    }
    if (WIFEXITED(status) || WIFSIGNALED(status)) {
      deletejob(myjobs, pid);
      Sigprocmask(SIG_SETMASK, &prev_all, NULL);
    } /* remove the job in job list accordingly */
  }
  errno = olderrno;
  return;
}

/*
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.
 */
void sigquit_handler(int sig) {
  int olderrno = errno; /* store old errno */
  sigset_t mask_all, prev_all;
  Sigfillset(&mask_all);
  Sigprocmask(SIG_BLOCK, &mask_all,
              &prev_all); /* block all signal in case the race between SIGINT
                             and SIGCHLD */

  pid_t pid;
  pid = fgpid(myjobs);
  if (pid > 0) {
    Kill(-pid, SIGINT); /* kill processes in fg job's process group */

    printf("Job [%d] (%d) terminated by signal %d\n", pid2jid(pid), pid,
           sig); /* print the message */

    deletejob(myjobs, pid); /* delete job in joblist */
  }
  Sigprocmask(SIG_SETMASK, &prev_all, NULL);

  errno = olderrno;
  return;
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.
 */
void sigstop_handler(int sig) {
  int olderrno = errno; /* store old errno */
  sigset_t mask_all, prev_all;
  Sigfillset(&mask_all);
  Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
  pid_t pid;
  pid = fgpid(myjobs);
  if (pid > 0) {
    Kill(-pid, SIGTSTP); /* send SIGTSTP to fg job's process group */

    printf("Job [%d] (%d) stopped by signal %d\n", pid2jid(pid), pid,
           sig); /* print the message */
    struct job_t *stpjob = getjobpid(myjobs, pid);
    stpjob->state = ST; /* modify the job list */
  }
  Sigprocmask(SIG_SETMASK, &prev_all, NULL);

  errno = olderrno;
  return;
}
