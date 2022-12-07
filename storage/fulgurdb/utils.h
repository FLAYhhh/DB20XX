#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <cstdio>
#include <iostream>
#include <memory>
#include <cstring>
#include <cassert>
#include <atomic>
#include "message_logger.h"


namespace fulgurdb {

/**
 *@brief <NOT SURE> FIXME
 */
class Latch {
  public:
    void lock() {
      while (flag.test_and_set(std::memory_order_acquire)) {}
    }

    void unlock() {
      flag.clear(std::memory_order_relaxed);
    }

  private:
    std::atomic_flag flag = ATOMIC_FLAG_INIT;
};

}
