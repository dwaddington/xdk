#ifndef __CPU_MASK_H__
#define __CPU_MASK_H__

#ifdef _GNU_SOURCE

#include "logging.h"
#include <sched.h>

class cpu_mask_t
{
private:
  cpu_set_t cpu_set_;

public:
  cpu_mask_t() {
    __builtin_memset(&cpu_set_,0,sizeof(cpu_set_t));
  }

  void set_bit(int cpu) {
    CPU_SET(cpu, &cpu_set_);
  }

  void set_mask(uint64_t mask) {
    int current = 0;
    while(mask > 0) {
      if(mask & 0x1ULL) {
        CPU_SET(current, &cpu_set_);
      }
      current++;
      mask = mask >> 1;
    }
  }

  const cpu_set_t * cpu_set() { 
    return &cpu_set_;
  }

  bool is_set() {
    return (CPU_COUNT(&cpu_set_) > 0);
  }

  void dump() {
    for(unsigned i=0;i<64;i++) {
      if(CPU_ISSET(i,&cpu_set_)) 
        printf("1");
      else 
        printf("0");
    }
    printf("\n");
  }

};

#endif

#endif
