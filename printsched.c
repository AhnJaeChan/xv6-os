//
// Created by 안재찬 on 2019-05-27.
//

#include "types.h"
#include "stat.h"
#include "user.h"

int main() {
//  printsched();
  char *args[3] = {"test_thread", "echo is executed!", 0};
  printsched();
  exec("test_thread", args);
  printf(1, "printsched?\n");
  exit();
}