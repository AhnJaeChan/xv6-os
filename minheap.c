//
// Created by 안재찬 on 2019-05-25.
//

#include "minheap.h"
#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"

void heap_init(heap_t *h) {
  for (int i = 0; i < MAX_HEAP_SIZE; ++i) {
    h->parr[i] = NULL;
  }
  h->share = 0;
  h->sz = 0;
}

int heap_push(heap_t *h, struct proc *p) {
  if (h->sz == MAX_HEAP_SIZE) {
    return -1;
  }

  h->sz++;
  int i = h->sz - 1;

  h->parr[i] = p;

  while (i != 0 && h->parr[heap_parent(i)]->stride_config.pass >= h->parr[i]->stride_config.pass) {
    swapproc(&h->parr[heap_parent(i)], &h->parr[i]);
    i = heap_parent(i);
  }

  return 0;
}

struct proc *heap_pop(heap_t *h) {
  if (heap_is_empty(h)) {
    return NULL;
  }

  if (h->sz == 1) {
    h->sz--;
    return h->parr[0];
  }

  struct proc *root = h->parr[0];
  h->parr[0] = h->parr[h->sz - 1];
  h->sz--;
  heapify(h, 0);

  return root;
}

struct proc *heap_peek(heap_t *h) {
  if (heap_is_empty(h)) {
    return NULL;
  }

  return h->parr[0];
}

uint heap_peek_pass(heap_t *h) {
  struct proc *p = heap_peek(h);

  if (p == NULL) {
    return INFINITE;
  } else if (p != mlfqproc && p->state != RUNNABLE) {
    return 0;
  }

  return p->stride_config.pass;
}

void heapify(heap_t *h, int i) {
  int left = heap_left(i);
  int right = heap_right(i);
  int smallest = i;

  if (left < h->sz && h->parr[left]->stride_config.pass < h->parr[i]->stride_config.pass) {
    smallest = left;
  }

  if (right < h->sz && h->parr[right]->stride_config.pass < h->parr[smallest]->stride_config.pass) {
    smallest = right;
  }

  if (smallest != i) {
    swapproc(&h->parr[smallest], &h->parr[i]);
    heapify(h, smallest);
  }
}

int heap_delete(heap_t *h, struct proc *p) {
  int idx = heap_search(h, p);

  if (idx < 0) {
    return -1;
  }

  heap_set_pass(h, idx, 0);
  heap_pop(h);

  return 0;
}

int heap_search(heap_t *h, struct proc *p) {
  for (int i = 0; i < h->sz; ++i) {
    if (h->parr[i] == p) {
      return i;
    }
  }

  return -1;
}

void heap_set_pass(heap_t *h, int i, uint pass) {
  if (pass < h->parr[i]->stride_config.pass) {
    h->parr[i]->stride_config.pass = pass;

    while (i != 0 && h->parr[heap_parent(i)]->stride_config.pass >= h->parr[i]->stride_config.pass) {
      swapproc(&h->parr[i], &h->parr[heap_parent(i)]);
      i = heap_parent(i);
    }
  } else {
    h->parr[i]->stride_config.pass = pass;

    heapify(h, i);
  }
}

int heap_parent(int i) {
  return (i - 1) / 2;
}

int heap_left(int i) {
  return 2 * i + 1;
}

int heap_right(int i) {
  return 2 * i + 2;
}

int heap_is_empty(heap_t *h) {
  return h->sz == 0;
}
