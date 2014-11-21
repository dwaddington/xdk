#ifndef __COMMON_SPINLOCK_H__
#define __COMMON_SPINLOCK_H__

#include <boost/atomic.hpp>

class Spinlock {
private:
  typedef enum {Locked, Unlocked} LockState;
  boost::atomic<LockState> state_;

public:
  Spinlock() : state_(Unlocked) {}

  void lock()
  {
    while (state_.exchange(Locked, boost::memory_order_acquire) == Locked) {
      /* busy-wait */
    }
  }
  void unlock()
  {
    state_.store(Unlocked, boost::memory_order_release);
  }
};

#endif
