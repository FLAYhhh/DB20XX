#pragma once
#include "return_status.h"
#include "utils.h"
#include "record_location.h"
#include "record_block.h"
#include "record.h"
namespace fulgurdb {


//FIXME
class TransactionContext {
public:
  void BeginTransaction(uint32_t thread_id, bool read_only = false) {
    TransactionContext *txn = nullptr;
    // if the isolation level is set to:
    // - SERIALIZABLE, or
    // - REPEATABLE_READS, or
    // - READ_COMMITTED.
    // transaction processing with decentralized epoch manager
    uint64_t read_id = EpochManagerFactory::GetInstance().EnterEpoch(
        thread_id, TimestampType1::READ);
    read_id_ = read_id;
    commit_id_ = commit_id;
    txn_id_ = commit_id_;
    epoch_id_ = read_id_ >> 32;
    thread_id_ = thread_id;
    is_written_ = false;
    timestamp_ = DateFunctions::Now();
    //isolation_level_ = isolation;

    gc_set_ = std::make_shared<GCSet>();
    gc_object_set_ = std::make_shared<GCObjectSet>();

    read_only_ = read_only;
  }

  void apply_insert(RecordLocation &rec_loc) {
    RecordHeader *rec_header = rec_loc.get_record_header();
    // txn_id is a write latch
    rec_header->set_transaction_id(txn_id_);
    rec_header->set_last_reader_commitid(commit_id_);

    // no need to set next item pointer.

    // Add the new tuple into the insert set
    rw_set_record_insert(rec_loc);
  }

  ReturnStatus get_transaction_status() {
    if (should_abort_)
      return FULGUR_TRANSACTION_ABORT;
    else
      return FULGUR_SUCCESS;
  }

  ReturnStatus commit() {
    // do something
    // TODO
    if (read_only_) {
      reset();
      return  FULGUR_SUCCESS;
    }

    // Read Own:
    // for read own records, we just need to free the latch
    for (auto &rloc : txn_read_own_set_) {
      RecordHeader *rloc_header = rloc.get_record_header();
      assert(rloc.get_record_header() != nullptr);
      rloc_header->set_transaction_id(INVALID_TRANSACTION_ID);
    }

    // Insert:
    // for insert records, we need to make it visiable,
    // free latch & set begin/end timestamp
    for (auto &rloc : txn_insert_set_) {
      RecordHeader *rloc_header = rloc.get_record_header();
      assert(rloc.get_record_header() != nullptr);

      rloc_header->set_begin_timestamp(commit_timestamp_);
      rloc_header->set_end_timestamp(MAX_TIMESTAMP);
      rloc_header->set_transaction_id(INVALID_TRANSACTION_ID);
    }

    // then reset status
    reset();
  }

  void abort() {

  }


private:
  void reset() {

  }

  void rw_set_record_insert(RecordLocation &rec_loc) {
    txn_insert_set_.insert(rec_loc);
  }

  void rw_set_record_update(RecordLocation &rec_loc) {
    txn_update_set_.insert(rec_loc);
  }

  void rw_set_record_delete(RecordLocation &rec_loc) {
    txn_delete_set_.insert(rec_loc);
  }

  void rw_set_record_read(RecordLocation &rec_loc) {
    txn_read_set_.insert(rec_loc);
  }


private:
  bool started_ = false;
  bool should_abort_ = false;
  uint32_t read_id_ = 0;
  uint32_t commit_id_ = 0;
  uint32_t txn_id_ = 0;
  uint32_t epoch_id_ = 0;
  uint32_t thread_id_ = 0;
  uint64_t timestamp_ = 0;
  bool is_written_ = 0;
  bool read_only_ = false;

  std::unordered_set<RecordLocation> txn_insert_set_;
  std::unordered_set<RecordLocation> txn_update_set_;
  std::unordered_set<RecordLocation> txn_delete_set_;
  std::unordered_set<RecordLocation> txn_read_set_;
};

}
