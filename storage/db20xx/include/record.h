#pragma once
#include <cstdint>
#include "data_types.h"
#include "return_status.h"
#include "schema.h"
#include "utils.h"
#include "version_chain.h"

namespace db20xx {

class RecordHeader {
 public:
  /*
   *  For several reasons:
   *  1. write last_read_ts_ depends on the value of txn_id_
   *  2. write txn_id_ depends on the value of last_read_ts_
   *  So we need to hold the latch when during check and
   *    modify these two fields
   *
   *  If a transaction have own the record(txn_id_ is set),
   *    there is no need to hold the latch, because no one except
   *    itself can modify the header.
   */
  Latch latch_;

  /**
   * When each transaction starts, system will assign it a unique
   * global timestamp. This unique global timestamp is used as transaction id.
   *
   * If a transaction wants to create or modify a record, it must put its
   * transaction id in this field.
   */
  uint64_t txn_id_ = INVALID_TRANSACTION_ID;

  /**
   * If a newer transaction reads one record, another older transaction
   * can not modify this record anymore.
   *
   * Hence we need to record the newest transaction id of all readers
   * to this record. To help us coordinate writer and reader of a record.
   *
   * last_read_ts_ is only useful for latest version
   * (begin_ts_ != MAX_TIMESTAMP && end_ts_ == MAX_TIMESTAMP)
   */
  uint64_t last_read_ts_ = INVALID_READ_TIMESTAMP;

  /**
   * [committed vs uncommited]
   * If begin_ts_ == MAX_TIMESTAMP, the record is uncommited.
   * Else, the record is committed.
   *
   * For a transaction with transaction id [txn_x]:
   * Committed records having (begin_ts_ <= txn_x < end_ts_) is visible to
   * the transaction.
   * Uncommited records having (txn_id_ == txn_x) is visible to the transaction
   *
   * If end_ts_ == 0, the record is deleted.
   */
  uint64_t begin_ts_ = MAX_TIMESTAMP;
  uint64_t end_ts_ = MAX_TIMESTAMP;

  /**
   * older_version_ points to a older record in the MVCC version chain.
   * newer_version_ points to a newer record in the MVCC version chain.
   */
  Record *older_version_ = nullptr;
  Record *newer_version_ = nullptr;

  /**
   * MVCC version chain is a linked list with a fake head.
   * vchain_head_ is the MVCC version chain head.
   *
   * The value type of Primary Index in db20xx is [VersionChainHead *].
   */
  VersionChainHead *vchain_head_ = nullptr;
};
//__attribute__((aligned(64)));

class Record {
  friend class TransactionContext;

 public:
  void init();
  void lock_header();
  void unlock_header();

  void set_transaction_id(uint64_t txn_id);
  void set_begin_timestamp(uint64_t begin_ts);
  void set_end_timestamp(uint64_t end_ts);
  void set_last_read_timestamp(uint64_t last_read_ts);
  void set_older_version(Record *record);
  void set_newer_version(Record *record);
  uint64_t get_transaction_id() const;
  uint64_t get_begin_timestamp() const;
  uint64_t get_end_timestamp() const;
  uint64_t get_last_read_timestamp() const;
  Record *get_newer_version();
  Record *get_older_version();
  void set_vchain_head(VersionChainHead *vchain_head);
  VersionChainHead *get_vchain_head();

  void load_data_from_mysql(char *mysql_record, const Schema &schema);
  void load_data_to_mysql(char *mysql_record, const Schema &schema);
  char *get_payload();
  RecordHeader *get_header();

 private:
  RecordHeader header_;
  char payload_[0];  // payload lenght is specified in table
};

}  // end of namespace db20xx
