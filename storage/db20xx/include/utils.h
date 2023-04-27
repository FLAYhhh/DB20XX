#pragma once
#include <cstdint>
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


namespace db20xx {

/**
 *@brief <NOT SURE> FIXME
 */
class Latch {
  friend class Record;
  public:
    void init() {
      flag_.clear(std::memory_order_relaxed);
    }

    void lock() {
      uint64_t cnt = 0;
      while (flag_.test_and_set(std::memory_order_acquire)) {
        cnt++;
        if (cnt % 10000 == 0) {
          LOG_TRACE("lock loop count:%lu", cnt);
        }
      }
    }

    void unlock() {
      flag_.clear(std::memory_order_relaxed);
    }

  private:
    std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
};

}
