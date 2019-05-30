//
// Created by 안재찬 on 2019-05-29.
//

#include "types.h"
#include "stat.h"
#include "user.h"

int cnt = 0;

void *foo() {
  cnt++;
  thread_exit(0);
  return 0;
}

int main() {
  thread_t t;
  thread_create(&t, foo, 0);
  printf(1, "thread id: %d created\n", t);
  thread_join(t, 0);
  printf(1, "cnt: %d\n", cnt);
  exit();
}