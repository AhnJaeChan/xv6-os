//
// Created by 안재찬 on 2019-05-25.
//

#include "types.h"
#include "param.h"
#include "mmu.h"
#include "minheap.h"
#include "proc.h"
#include "stride.h"

void stride_init_config(stride_config_t *config) {
  stride_set_ticket(config, TICKET1);
  config->share = 0;
  config->pass = 0;
}

void stride_set_ticket(stride_config_t *config, uint ticket) {
  config->ticket = ticket;
  config->stride = STRIDE1 / ticket;
}

void stride_set_share(struct proc *p, uint share) {
  p->stride_config.share = share;
  p->type = STRIDE;
}

void stride_rearrange(heap_t *h) {
  int i;
  uint unit = stride_count_default_tickets(h) / (100 - h->share);

  for (i = 0; i < h->sz; ++i) {
    if (h->parr[i]->type == STRIDE) {
      stride_set_ticket(&h->parr[i]->stride_config, unit * h->parr[i]->stride_config.share);
    }
  }

  heapify(h, 0);
}

uint stride_count_default_tickets(heap_t *h) {
  int i, cnt = 0;

  for (i = 0; i < h->sz; ++i) {
    if (h->parr[i]->type == DEFAULT) {
      cnt += h->parr[i]->stride_config.ticket;
    }
  }

  return cnt;
}