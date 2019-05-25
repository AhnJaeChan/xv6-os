//
// Created by 안재찬 on 2019-05-25.
//

#ifndef XV6_PUBLIC_STRIDE_H
#define XV6_PUBLIC_STRIDE_H

#include "types.h"

#define STRIDE1   1048576 // 1 << 20
#define TICKET1   1024
#define MAX_SHARE  20

// Stride struct
typedef struct stride_t {
  uint ticket;
  uint stride;
  uint pass;
  uint percentage;
} stride_t;

void stride_set_default(stride_t *);
void stride_set_max(stride_t *);

#endif //XV6_PUBLIC_STRIDE_H
