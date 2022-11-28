#pragma once
#include "./masstree-beta/kvthread.hh"
#include "./index.h"
#include "transaction.h"

namespace fulgurdb {

typedef threadinfo threadinfo_type;

class ThreadLocal {
friend class Table;
public:
  ThreadLocal(uint32_t thread_id): thread_id_(thread_id) {
    ti_ = threadinfo::make(threadinfo::TI_PROCESS, thread_id);
  }

  ~ThreadLocal() {}

  threadinfo *get_threadinfo() const {
    return ti_;
  }

  scan_stack_type &ref_masstree_scan_stack() {
    return masstree_scan_stack_;
  }

  void reset_masstree_scan_stack() {
    masstree_scan_stack_.reset();
  }

  uint32_t get_thread_id() {
    return thread_id_;
  }

  TransactionContext *get_transaction_context() {
    return &txn_ctx_;
  }

private:
  // logic thread id, get from mysql:current_thd->thread_id()
  uint32_t thread_id_ = 0;
  threadinfo *ti_ = nullptr;
  //FIXME: move to handle
  scan_stack_type masstree_scan_stack_;
  TransactionContext txn_ctx_;
};

}
