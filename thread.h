//
// Created by 안재찬 on 2019-05-28.
//

#ifndef XV6_PUBLIC_THREAD_H
#define XV6_PUBLIC_THREAD_H

#define MAX_THREADS 8

struct proc;

enum threadstate {
  FREE, WORKING
};

typedef struct thread_config_t {
  uint sp;
  int valid;
  enum threadstate state;
} thread_config_t;

void thread_init_config(thread_config_t *);
thread_config_t *thread_alloc_config(struct proc *);

#endif //XV6_PUBLIC_THREAD_H
