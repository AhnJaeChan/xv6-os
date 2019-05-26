//
// Created by 안재찬 on 2019-05-26.
//

#include "mlfq.h"
#include "mmu.h"
#include "proc.h"

void mlfq_init(mlfq_t *mlfq) {
  queue_init(&mlfq->queue[0], 1, 5);
  queue_init(&mlfq->queue[1], 2, 10);
  queue_init(&mlfq->queue[2], 4, INFINITE);

  mlfq->ticks = 0;
}

void mlfq_push(mlfq_t *mlfq, struct proc *p) {
  enqueue(&mlfq->queue[0], p);
}

struct proc *mlfq_pop(mlfq_t *mlfq) {
  int i;

  for (i = 0; i < MLFQ_LEVELS; ++i) {
    if (mlfq->queue[i].size > 0) {
      return dequeue(&mlfq->queue[i]);
    }
  }
  return NULL;
}

int mlfq_delete(mlfq_t *mlfq, struct proc *p) {
  queue_t *q;

  for (q = mlfq->queue; q < &mlfq->queue[MLFQ_LEVELS]; ++q) {
    if (queue_delete(q, p) == 0) {
      return 0;
    }
  }
  return -1;
}

void mlfq_init_config(mlfq_config_t *config) {
  config->level = 0;
  config->quantum = 0;
  config->allotment = 0;
}