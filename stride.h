//
// Created by 안재찬 on 2019-05-25.
//

#ifndef XV6_PUBLIC_STRIDE_H
#define XV6_PUBLIC_STRIDE_H

#include "minheap.h"

#define STRIDE1           1048576 // 1 << 20
#define TICKET1           1024 // 1 << 18
#define MAX_STRIDE_SHARE  20

typedef struct stride_config_t {
  uint ticket;
  uint stride;
  uint pass;
  uint share;
} stride_config_t;

void stride_init_config(stride_config_t *);
void stride_set_ticket(stride_config_t *, uint);
void stride_set_share(struct proc *, uint);
void stride_rearrange(heap_t *);
uint stride_count_default_tickets(heap_t *);

#endif //XV6_PUBLIC_STRIDE_H
