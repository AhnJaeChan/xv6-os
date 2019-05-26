//
// Created by 안재찬 on 2019-05-25.
//

#include "stride.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"

void stride_set_ticket(stride_t *s, uint ticket) {
  s->ticket = ticket;
  s->stride = STRIDE1 / ticket;
}

void stride_set_share(struct proc *p, uint share) {
  p->config.share = share;
  p->type = STRIDE;
}

void stride_rearrange(heap_t *h) {
  int i;
  uint unit = stride_count_default_tickets(h) / (100 - h->share);

  for (i = 0; i < h->sz; ++i) {
    if (h->parr[i]->type == STRIDE) {
      stride_set_ticket(&h->parr[i]->config, unit * h->parr[i]->config.share);
    }
  }

  heapify(h, 0);
}

uint stride_count_default_tickets(heap_t *h) {
  int i, cnt = 0;

  for (i = 0; i < h->sz; ++i) {
    if (h->parr[i]->type == DEFAULT) {
      cnt += h->parr[i]->config.ticket;
    }
  }

  return cnt;
}