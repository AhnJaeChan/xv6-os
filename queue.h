//
// Created by 안재찬 on 2019-05-26.
//

#ifndef XV6_PUBLIC_QUEUE_H
#define XV6_PUBLIC_QUEUE_H

#define QUEUE_SIZE NPROC

// Static queue
typedef struct queue_t {
  struct proc *parr[QUEUE_SIZE];
  uint front;
  uint size;

  uint quantum;
  uint allotment;
} queue_t;

void queue_init(queue_t *, uint, uint);
void queue_clear(queue_t *);

int enqueue(queue_t *, struct proc *);
struct proc *dequeue(queue_t *);

int queue_delete(queue_t *, struct proc *);
int queue_search(queue_t *, struct proc *);
struct proc *queue_fetch(queue_t *, int);

#endif //XV6_PUBLIC_QUEUE_H
