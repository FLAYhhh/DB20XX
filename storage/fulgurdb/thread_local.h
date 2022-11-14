#pragma once
#include "./masstree-beta/kvthread.hh"
#include "./index.h"

namespace fulgurdb {

typedef threadinfo threadinfo_type;

class ThreadLocal {
friend class Table;
public:
  ThreadLocal(uint32_t thread_id) {
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


private:
  threadinfo *ti_ = nullptr;
  scan_stack_type masstree_scan_stack_;
};

}
