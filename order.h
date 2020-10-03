#ifndef _ORDER_
#define _ORDER_
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>

struct order {
  pid_t PID; // The PID of the "customer"
  // 0 : "overgrips"; 1 : "racket strings"; 2: "tennis shoes"; 3: "tennis balls"; 4: "tennis accessories"
  bool numItems [5];
  bool shipped;
  int orderNumber;
};

#endif
