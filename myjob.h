#ifndef MY_JOB_H
#define MY_JOB_H
#include <pthread.h>

#include "csapp.h"
#define jid_t pid_t
#define MAXJOBS 1024
#define UNDEF 0 /* undefined */
#define FG 1    /* running in foreground */
#define BG 2    /* running in background */
#define ST 3    /* stopped */
#define MAXPIPE 1024

struct job_t {            /* The job struct */
  pid_t pid[MAXPIPE + 1]; /* job PID */
  int pid_num;
  int jid;               /* job ID [1, 2, ...] */
  int state;             /* UNDEF, BG, FG, or ST */
  char cmdline[MAXLINE]; /* command line */
};

int pid2jid(pid_t pid);
void initjobs(struct job_t* jobs);
void listjobs(struct job_t* jobs);
void clearjob(struct job_t* job);
int maxjid(struct job_t* jobs);
int addjob(struct job_t* jobs, pid_t* pid, int pid_num, int state,
           char* cmdline);
int deletejob(struct job_t* jobs, pid_t pid);
pid_t fgpid(struct job_t* jobs);
struct job_t* getjobjid(struct job_t* jobs, int jid);
struct job_t* getjobpid(struct job_t* jobs, int pid);
#endif