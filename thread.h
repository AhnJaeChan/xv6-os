//
// Created by 안재찬 on 2019-05-28.
//

#ifndef XV6_PUBLIC_THREAD_H
#define XV6_PUBLIC_THREAD_H

#define MAX_THREADS NPROC

struct proc;

enum threadstate {
  FREE, WORKING
};

typedef struct thread_config_t {
  uint sp;
  struct proc *thread;
  int valid;
  enum threadstate state;
  void *retval;
} thread_config_t;

void thread_init_config(thread_config_t *);
thread_config_t *thread_alloc_config(struct proc *);

int thread_create(thread_t *, void *(void *), void *);
int thread_join(thread_t, void **);
void thread_exit(void *);
int thread_kill_all(struct proc *);
int thread_clear(struct proc *);

#endif //XV6_PUBLIC_THREAD_H
