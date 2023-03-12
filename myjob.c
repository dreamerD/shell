#include "myjob.h"

struct job_t myjobs[MAXJOBS]; /* The job list */

int pid2jid(pid_t pid) {
  int i;
  if (pid < 1) {
    return 0;
  }
  for (int i = 0; i < MAXJOBS; i++) {
    /* code */
    for (int j = 0; j < myjobs[i].pid_num; j++) {
      if (myjobs[i].pid[j] == pid) {
        return myjobs[i].jid;
      }
    }
  }
}

void listjobs(struct job_t* jobs) {
  int i;

  for (i = 0; i < MAXJOBS; i++) {
    if (jobs[i].pid[0] != 0) {
      printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid[0]);
      switch (jobs[i].state) {
        case BG:
          printf("Running ");
          break;
        case FG:
          printf("Foreground ");
          break;
        case ST:
          printf("Stopped ");
          break;
        default:
          printf("listjobs: Internal error: job[%d].state=%d ", i,
                 jobs[i].state);
      }
      printf("%s", jobs[i].cmdline);
    }
  }
}
void initjobs(struct job_t* jobs) {
  int i;

  for (i = 0; i < MAXJOBS; i++) clearjob(&jobs[i]);
}

void clearjob(struct job_t* job) {
  job->pid_num = 0;
  job->jid = 0;
  job->state = UNDEF;
  job->cmdline[0] = '\0';
}

int maxjid(struct job_t* jobs) {
  int i, max = 0;

  for (i = 0; i < MAXJOBS; i++)
    if (jobs[i].jid > max) max = jobs[i].jid;
  return max;
}

int addjob(struct job_t* jobs, pid_t* pid, int pid_num, int state,
           char* cmdline) {
  int i;
  for (i = 0; i < MAXJOBS; i++) {
    if (jobs[i].state == UNDEF) {
      for (int j = 0; j < pid_num; j++) {
        jobs[i].pid[j] = pid[j];
      }
      jobs[i].pid_num = pid_num;
      jobs[i].state = state;
      jobs[i].jid = maxjid(jobs) + 1;
      strcpy(jobs[i].cmdline, cmdline);
      return 1;
    }
  }
  return 0;
}

int deletejob(struct job_t* jobs, pid_t pid) {
  int i;

  if (pid < 1) return 0;

  for (i = 0; i < MAXJOBS; i++) {
    for (int j = 0; j < jobs[i].pid_num; j++) {
      if (jobs[i].pid[j] == pid) {
        jobs[i].pid_num--;
        if (jobs[i].pid_num == 0) {
          clearjob(&jobs[i]);
        }
        return 1;
      }
    }
  }
  return 0;
}

pid_t fgpid(struct job_t* jobs) {
  int i;

  for (i = 0; i < MAXJOBS; i++)
    if (jobs[i].state == FG) return jobs[i].pid[0];
  return 0;
}

struct job_t* getjobjid(struct job_t* jobs, int jid) {
  int i;

  if (jid < 1) return NULL;
  for (i = 0; i < MAXJOBS; i++)
    if (jobs[i].jid == jid) return &jobs[i];
  return NULL;
}

struct job_t* getjobpid(struct job_t* jobs, int pid) {
  int i;

  if (pid < 1) return NULL;
  for (i = 0; i < MAXJOBS; i++) {
    for (int j = 0; j < jobs[i].pid_num; j++) {
      if (jobs[i].pid[j] == pid) return &jobs[i];
    }
  }
  return NULL;
}