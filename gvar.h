//
// Created by 안재찬 on 2019-05-29.
//

#ifndef XV6_PUBLIC_GVAR_H
#define XV6_PUBLIC_GVAR_H

typedef struct ptable_t {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable_t;

ptable_t ptable;
mlfq_t pmlfq;
heap_t pheap;
struct proc *mlfqproc;
struct proc *initproc;

#endif //XV6_PUBLIC_GVAR_H
