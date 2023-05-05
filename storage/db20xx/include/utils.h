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
#include <mutex>
#include <shared_mutex>


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

/**
 * Reader-Writer latch backed by std::mutex.
 */
class ReaderWriterLatch {
 public:
  /**
   * Acquire a write latch.
   */
  void WLock() { mutex_.lock(); }

  /**
   * Release a write latch.
   */
  void WUnlock() { mutex_.unlock(); }

  /**
   * Acquire a read latch.
   */
  void RLock() { mutex_.lock_shared(); }

  /**
   * Release a read latch.
   */
  void RUnlock() { mutex_.unlock_shared(); }

 private:
  std::shared_mutex mutex_;
};

}
