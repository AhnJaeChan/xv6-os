//
// Created by 안재찬 on 2019-05-26.
//

#include "queue.h"
#include "mmu.h"
#include "proc.h"

void queue_init(queue_t *q, uint quantum, uint allotment) {
  queue_clear(q);

  q->quantum = quantum;
  q->allotment = allotment;
}

void queue_clear(queue_t *q) {
  q->front = q->size = 0;
}

int enqueue(queue_t *q, struct proc *p) {
  if (q->size == QUEUE_SIZE) {
    return -1;
  }

  p->type = MLFQ;

  q->parr[(q->front + q->size) % QUEUE_SIZE] = p;
  q->size++;

  return 0;
}

struct proc *dequeue(queue_t *q) {
  struct proc *p;

  if (q->size == 0) {
    return NULL;
  }

  p = q->parr[q->front];
  q->front = (q->front + 1) % QUEUE_SIZE;
  q->size--;

  return p;
}

int queue_delete(queue_t *q, struct proc *p) {
  int idx, i;

  if (q == NULL || p == NULL || q->size == 0) {
    return -1;
  }

  if ((idx = queue_search(q, p)) == INFINITE) {
    return -1;
  }

  if (idx == q->front) {
    dequeue(q);
  } else {
    // Shift left
    for (i = idx; i < q->size - (q->front + idx); ++i) {
      q->parr[i % QUEUE_SIZE] = q->parr[(i + 1) % QUEUE_SIZE];
    }
    q->size--;
  }

  return 0;
}

int queue_search(queue_t *q, struct proc *p) {
  int i;

  for (i = 0; i < q->size; ++i) {
    if (q->parr[(q->front + i) % QUEUE_SIZE] == p) {
      return (q->front + i) % QUEUE_SIZE;
    }
  }
  return INFINITE;
}