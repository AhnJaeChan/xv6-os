//
// Created by 안재찬 on 2019-05-25.
//

#ifndef XV6_PUBLIC_MINHEAP_H
#define XV6_PUBLIC_MINHEAP_H

#define MAX_HEAP_SIZE NPROC

// minheap
typedef struct heap_t {
  struct proc *parr[MAX_HEAP_SIZE]; // Proc array
  uint share;
  uint sz;
} heap_t;

void heap_init(heap_t *);

int heap_push(heap_t *, struct proc *);
struct proc *heap_pop(heap_t *);
struct proc *heap_peek(heap_t *);
uint heap_peek_pass(heap_t *);

void heapify(struct heap_t *, int);

int heap_delete(heap_t *, struct proc *);
int heap_search(heap_t *, struct proc *);
void heap_set_pass(heap_t *, int, uint);

int heap_parent(int i);
int heap_left(int i);
int heap_right(int i);
int heap_is_empty(heap_t *);

#endif //XV6_PUBLIC_MINHEAP_H
