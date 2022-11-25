#pragma once
#include "./utils.h"
#include "record_location.h"

namespace fulgurdb {

class RecordHeader {
public:
  void set_transaction_id(uint64_t txn_id) {
    txn_id_ = txn_id;
  }

  void set_last_reader_commitid(uint64_t commit_id) {
    (void) commit_id;
    //FIXME
  }

private:
  uint32_t latch_; //TODO: CAS operation
  std::atomic<uint64_t> txn_id_;
  uint64_t read_ts_;
  uint64_t begin_ts_;
  uint64_t end_ts_;
  RecordLocation older_;
  RecordLocation newer_;
  RecordLocation *indirection_;
} __attribute__((aligned(64)));

class Record {
private:
  RecordHeader header_;
  char payload_[0]; //payload lenght is specified in table
};

} // end of namespace fulgurdb
