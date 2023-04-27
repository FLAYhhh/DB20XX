#pragma once
#include <sys/types.h>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include "utils.h"
#include "data_types.h"

namespace fulgurdb {

class Epoch {
public:
  Epoch(uint64_t epoch_id, uint64_t transaction_count)
    : epoch_id_(epoch_id),
      transaction_count_(transaction_count){}

public:
  uint64_t epoch_id_;
  uint64_t transaction_count_;
};

/**
 *@brief <NOT SURE> FIXME
 *  Several threads that have common mod belong to one LocalEpochManager;
 *
 *  LocalEpoch has a set of epochs with increasing epoch id, we also has an
 *  extra variable expoch_id_lower_bound_ to record the lowbound.
 *
 *  Each epoch has many transactions, we only record transaction numbers in
 *  each epoch.
 */
class LocalEpochManager {
public:
  bool enter_epoch(uint64_t epoch_id) {
    if (epoch_id_lower_bound_ == INVALID_EPOCH_ID) {
      // first time for this thread to enter epoch,
      // update epoch low bound
      // (epoch_id_lower_bound_, ...) <NOT SURE>
      epoch_id_lower_bound_ = epoch_id - 1;
    } else if (epoch_id_lower_bound_ > epoch_id) {
      //FIXME: what condition will reach here
      //see peloton
      return false;
    } else {
      // epoch_id_lower_bound_ <= new_epoch_id
      // do nothing
    }

    // at this point, epoch_id_lower_bound_ <= new_epoch_id
    auto epoch_itr = epochs_.find(epoch_id);
    if (epoch_itr == epochs_.end()) {
      Epoch *new_epoch = new Epoch(epoch_id, 1);
      epochs_[epoch_id] = new_epoch;
    } else {
      epoch_itr->second->transaction_count_++;
    }
    return true;
  }

private:
  uint64_t epoch_id_lower_bound_ = INVALID_EPOCH_ID;
  std::unordered_map<uint64_t, Epoch*> epochs_;
  //TODO epoch queue
};

class GlocalEpochManager {
public:
  /**
   * @brief
   *   Change the state of local epoch,
   *   and return a transaction id in current global epoch
   */
  static uint64_t enter_epoch(uint64_t thread_id) {
    /*
    while (true) {
      uint64_t cur_global_epoch_id = get_current_global_epoch_id();
      bool ret = local_epochs_[thread_id]->enter_epoch(cur_global_epoch_id);
      if (ret) {
        uint32_t next_txn_id = get_next_global_transaction_id();
        return (cur_global_epoch_id << 32) | next_txn_id;
      }
    }
    */
    // FIXME: do not use epoch for now
    (void) thread_id;
    return current_global_epoch_id_.fetch_add(1, std::memory_order_relaxed);
  }

  static uint64_t get_current_global_epoch_id() {
    return current_global_epoch_id_.load();
  }

  static uint32_t get_next_global_transaction_id() {
    return next_global_txn_id_.fetch_add(1, std::memory_order_relaxed);
  }

private:
  //only least significant 32 bits is used,
  //because we will do left shift(<< 32) to current_global_epoch_id_
  static std::atomic<uint64_t> current_global_epoch_id_;
  static std::mutex local_epochs_lock_;
  static std::unordered_map<int, LocalEpochManager*> local_epochs_;

  static std::atomic<uint32_t> next_global_txn_id_;
};

}
