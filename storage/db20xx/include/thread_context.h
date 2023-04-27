#pragma once
#include "masstree-beta/kvthread.hh"
#include "transaction.h"

namespace db20xx {

using namespace Masstree;
typedef threadinfo threadinfo_type;

class ThreadContext {
  friend class Table;
  //friend class Index;
  friend class MasstreeIndex;
  friend class ha_db20xx;

 public:
  ThreadContext(uint64_t thread_id) : thread_id_(thread_id) {
    ti_ = threadinfo::make(threadinfo::TI_PROCESS, thread_id);
  }
  ~ThreadContext() {}
  threadinfo *get_threadinfo() const { return ti_; }
  uint64_t get_thread_id() { return thread_id_; }
  TransactionContext *get_transaction_context() { return &txn_ctx_; }
  char *get_key_container() { return key_container_; }

 private:
  // logic thread id, get from mysql:current_thd->thread_id()
  uint64_t thread_id_ = 0;
  threadinfo *ti_ = nullptr;
  TransactionContext txn_ctx_;

  // avoid malloc when build temporary index key
  char key_container_[DB20XX_MAX_KEY_LENGTH];
};

}  // namespace db20xx
