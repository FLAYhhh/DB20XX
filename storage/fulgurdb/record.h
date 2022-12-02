#pragma once
#include <cstdint>
#include "./utils.h"
#include "data_types.h"
#include "index_indirection.h"
#include "record_location.h"
#include "return_status.h"
#include "transaction.h"

namespace fulgurdb {

class RecordHeader {
friend class TransactionContext;
public:
  void set_transaction_id(uint64_t txn_id) {
    txn_id_ = txn_id;
  }

  /**
   * @brief
   *   Record the reader with largest ts in [last_read_ts_]
   *   Fulgurdb uses a pessimistic approch to do version control,
   *     and set_last_read_timestamp() has some constrains with
   *     txn_id_ in the record header. Hence we must hold
   *     the latch of the record header during this funciton.
   */
  int set_last_read_timestamp(uint64_t reader_ts) {
    latch_.lock();
    if (end_ts_ < reader_ts)
      return FULGUR_FAIL;
    // Case1: no one own the version
    if (txn_id_ == INVALID_TRANSACTION_ID) {
      if (last_read_ts_ < reader_ts)
        last_read_ts_ = reader_ts;
    // Case2: An older transaction has owned the version
    // TODO: since the owner will set the last_read_ts_ to
    // its txn_id, we don't need to do anything here.
    } else if (reader_ts < txn_id_) {
      if (last_read_ts_ < reader_ts)
        last_read_ts_ = reader_ts;
    } else if (txn_id_ < reader_ts) {
      latch_.unlock();
      return false;
    }

    latch_.unlock();
  }

  void set_begin_timestamp(uint64_t begin_ts) {
    begin_ts_ = begin_ts;
  }

  void set_end_timestamp(uint64_t end_ts) {
    end_ts_ = end_ts;
  }

  void set_indirection(RecordPtr *p_record_ptr) {
    indirection_ = p_record_ptr;
  }

  uint64_t get_transaction_id() const {
    return txn_id_;
  }

  uint64_t get_begin_timestamp() const {
    return begin_ts_;
  }

  uint64_t get_end_timestamp() const {
    return end_ts_;
  }

  //@comment: be careful, initialization must be correct
  void init() {
    // latch_ has default ctor
    txn_id_ = INVALID_TRANSACTION_ID;
    last_read_ts_ = INVALID_READ_TIMESTAMP;
    begin_ts_ = MAX_TIMESTAMP;
    end_ts_ = MAX_TIMESTAMP;
    older_.init();
    newer_.init();
    indirection_ = nullptr;
  }

private:
  Latch latch_;
  uint64_t txn_id_;
  uint64_t last_read_ts_;
  uint64_t begin_ts_;
  uint64_t end_ts_;
  Record *older_;
  Record *newer_;
  RecordPtr *indirection_;
} __attribute__((aligned(64)));

class Record {
public:
  void init_header() {
    header_.init();
  }

  char *get_record_payload() {
    return payload_;
  }

  RecordHeader *get_record_header() {
    return &header_;
  }

  void set_indirection(RecordPtr *p_record_ptr) {
    header_.set_indirection(p_record_ptr);
  }

private:
  RecordHeader header_;
  char payload_[0]; //payload lenght is specified in table
};

} // end of namespace fulgurdb
