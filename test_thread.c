//
// Created by 안재찬 on 2019-05-29.
//

#include "types.h"
#include "stat.h"
#include "user.h"

int cnt = 0;

void *foo() {
  int ret;

  cnt++;
  thread_exit(&ret);

  printsched();

  return 0;
}

int main() {
  thread_t t;
  int result;
  if ((result = thread_create(&t, foo, 0)) > 0) {
    printf(1, "[%d] thread id: %d created\n", result, t);
  }

  while (1);
//  thread_join(t, 0);
  printf(1, "cnt: %d\n", cnt);
  exit();
}