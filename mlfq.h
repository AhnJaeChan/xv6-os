//
// Created by 안재찬 on 2019-05-26.
//

#ifndef XV6_PUBLIC_MLFQ_H
#define XV6_PUBLIC_MLFQ_H

#include "queue.h"

#define MLFQ_SHARE       20
#define MLFQ_LEVELS       3

typedef struct mlfq_t {
  queue_t queue[MLFQ_LEVELS];
  uint ticks;
} mlfq_t;

void mlfq_init(mlfq_t *);
void mlfq_push(mlfq_t *, struct proc *);
struct proc *mlfq_pop(mlfq_t *);
int mlfq_delete(mlfq_t *, struct proc *);

typedef struct mlfq_config_t {
  uint level;
  uint quantum;
  uint allotment;
} mlfq_config_t;

void mlfq_init_config(mlfq_config_t *);

#endif //XV6_PUBLIC_MLFQ_H
