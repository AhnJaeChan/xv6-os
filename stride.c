//
// Created by 안재찬 on 2019-05-25.
//

#include "stride.h"
#include "param.h"

void stride_set_default(stride_t *s) {
  s->ticket = TICKET1;
  s->stride = STRIDE1 / TICKET1;
}

void stride_set_max(stride_t *s) {
  s->ticket = 0;
  s->stride = 0;
  s->pass = INFINITE;
}
