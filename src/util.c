#include "util.h"

unsigned long time_difference_ns(stimespec* a, stimespec* b) {
  unsigned long sec_a = a->tv_sec;
  unsigned long sec_b = b->tv_sec;
  unsigned long ns_a = a->tv_nsec;
  unsigned long ns_b = b->tv_nsec;

  unsigned long s_tot = sec_a - sec_b;    // Calculating seconds difference
  unsigned long ns_tot = ns_a - ns_b;     // Calculating nanoseconds difference
  // printf("return value: %d\n", (S_TO_NS(s_tot) + (ns_tot)));   // TODO remove
  return (S_TO_NS(s_tot) + (ns_tot));     // Returns the time difference
}

short time_compare(stimespec* a, stimespec* b) {
  unsigned long sec_a = a->tv_sec;
  unsigned long sec_b = b->tv_sec;
  unsigned long ns_a = a->tv_nsec;
  unsigned long ns_b = b->tv_nsec;

  if(sec_a>sec_b) return -1;
  else if(sec_a<sec_b) return 1;
  else {
    if(ns_a>ns_b) return -1;
    else if(ns_a<ns_b) return 1;
    else return 0;
  }
}