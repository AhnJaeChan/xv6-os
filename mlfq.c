//
// Created by 안재찬 on 2019-05-26.
//

#include "types.h"
#include "param.h"
#include "mmu.h"
#include "queue.h"
#include "mlfq.h"
#include "proc.h"

void mlfq_init(mlfq_t *mlfq) {
  queue_init(&mlfq->queue[0], 1, 5);
  queue_init(&mlfq->queue[1], 2, 10);
  queue_init(&mlfq->queue[2], 4, INFINITE);

  mlfq->ticks = 0;
}

void mlfq_push(mlfq_t *mlfq, struct proc *p) {
  p->type = MLFQ;
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

void mlfq_downlevel(mlfq_t *mlfq, struct proc *p) {
  p->mlfq_config.quantum = 0;
  p->mlfq_config.allotment = 0;

  p->mlfq_config.level++;
  if (p->mlfq_config.level >= MLFQ_LEVELS) {
    p->mlfq_config.level = MLFQ_LEVELS - 1;
  }

  enqueue(&mlfq->queue[p->mlfq_config.level], p);
}

void mlfq_round_robin(mlfq_t *mlfq, struct proc *p) {
  p->mlfq_config.quantum = 0;
  enqueue(&mlfq->queue[p->mlfq_config.level], p);
}

void mlfq_boost(mlfq_t *mlfq) {
  struct proc *p;
  queue_t *q;
  int i;
  uint size;

  for (q = mlfq->queue; q < &mlfq->queue[MLFQ_LEVELS]; ++q) {
    size = q->size;
    for (i = 0; i < size; ++i) {
      p = dequeue(q);
      mlfq_init_config(&p->mlfq_config);
      enqueue(&mlfq->queue[0], p);
    }
  }
  mlfq->ticks = 0;
}

void mlfq_init_config(mlfq_config_t *config) {
  config->level = 0;
  config->quantum = 0;
  config->allotment = 0;
}