#include "transaction.h"
#include "record.h"
#include "record_location.h"
#include "table.h"
#include "thread_local.h"
namespace fulgurdb {

//======================public member function=========================
void TransactionContext::begin_transaction(uint64_t thread_id) {
  transaction_id_ = GlocalEpochManager::enter_epoch(thread_id);
  epoch_id_ = transaction_id_ >> 32;
  thread_id_ = thread_id;
}

// FIXME
void TransactionContext::mvto_insert(Record *record, Table *table, ThreadContext *thd_ctx) {
  // Alloc version chain head & insert it to index
  uint32_t writer_idx = thd_ctx->get_thread_id() % Table::PARALLEL_WRITER_NUM;
  threadinfo *ti = thd_ctx->get_threadinfo();
  VersionChainHeadBlock *vchain_head_block = nullptr;
  VersionChainHead *vchain_head = nullptr;  // pointer to RecordPtr
  int status = FULGUR_SUCCESS;
  do {
    vchain_head_block = table->vchain_head_allocators_[writer_idx];
    status = vchain_head_block->alloc_vchain_head(vchain_head);
  } while (status != FULGUR_SUCCESS);

  if (vchain_head_block->is_last_vchain_head(vchain_head)) {
    table->vchain_head_allocators_[writer_idx] = table->alloc_vchain_head_block();
  }

  record->set_vchain_head(vchain_head);
  record->set_transaction_id(transaction_id_);
  record->set_last_read_timestamp(transaction_id_);
  add_to_insert_set(record);

  // We need to insert uncommited record to index,
  // so that subsequent queries in the same transaction
  // can find it from index
  table->insert_record_to_index(vchain_head, *ti);
}

int TransactionContext::mvto_update(Record *old_record, char *new_mysql_record,
                                    Table *table, ThreadContext *thd_ctx) {
  // current transaction already has the ownership of old record version
  // this happens in two conditios:
  // 1. ownership is got by read_own(table scan or index read)
  // 2  ownership is got by last update operation in the same transaction
  if (old_record->get_transaction_id() == transaction_id_) {
    if (in_update_set(old_record)) {
      //old_record->unlock_header();
      Record *new_record = old_record->get_newer_version();
      new_record->load_data_from_mysql(new_mysql_record, table->schema_);
      return FULGUR_SUCCESS;
    } else {
      Record *new_record = nullptr;
      int status = table->alloc_record(new_record, thd_ctx);
      if (status != FULGUR_SUCCESS) {

        return status;
      }
 
      new_loc.load_data_from_mysql(new_mysql_record, table->schema_);
 
      RecordHeader *old_rec_header = old_rec_loc.get_record_header();
      RecordHeader *new_rec_header = new_loc.get_record_header();
      new_rec_header->set_vchain_head(old_rec_header->get_vchain_head());
      add_to_update_set(new_loc);  // FIXME
    }

  } else {
    // Panic: assume update always need read_own
    assert(false);
    return FULGUR_FAIL;
  }

  // never reach here
  assert(false);
  return FULGUR_FAIL;
}

/**
 *@brief
 *  apply_read() and get_visibility() are used for table scan
 *  without index
 */
int TransactionContext::mvto_read(RecordLocation &rec_loc, bool read_own) {
  if (read_own)
    return apply_read_own(rec_loc);
  else
    return apply_read_not_own(rec_loc);
}

/**
 * FIXME: consider effects of update/delete operations
 * TODO: move visibility judge to apply read
 *@brief
 *  currently, get_visibility() only works for read operation
 */
int TransactionContext::get_visibility(RecordLocation &rec_loc) {
  // FIXME we do not hold lock of this record version,
  //       what if its header is changed by other threads
  //       during current thread's execution?
  RecordHeader *rec_header = rec_loc.get_record_header();
  uint64_t record_begin_ts = rec_header->get_begin_timestamp();
  uint64_t record_end_ts = rec_header->get_end_timestamp();
  uint64_t record_txn_id = rec_header->get_transaction_id();
  bool ownership = (transaction_id_ == record_txn_id);

  bool committed_version = (transaction_id_ >= record_begin_ts);
  bool invalidated_version = (transaction_id_ >= record_end_ts);

  if (ownership == true) {
    // condition: current transaction holds this record version
    // sub-case1: this is a newly inserted/updated record version
    if (record_begin_ts == MAX_TIMESTAMP && record_end_ts == MAX_TIMESTAMP) {
      return RecordVersionVisibility::VISIBLE;
      // sub-case2: TODO (version in read_own set)
    } else if (record_end_ts == INVALID_TIMESTAMP) {
      return RecordVersionVisibility::DELETED;
    } else {
      return RecordVersionVisibility::INVISIBLE;
    }
  } else {
    // condition: current transaction doesn't hold this record version
    // sub-case1: this record version is owned by other transaction
    if (record_txn_id != INVALID_TRANSACTION_ID) {
      // sub-sub-case1: this version has not been commited by another txn
      if (record_begin_ts == MAX_TIMESTAMP) {
        return RecordVersionVisibility::INVISIBLE;
      } else {  // sub-sub-case2: has been commited by another txn
        if (committed_version && !invalidated_version) {
          return RecordVersionVisibility::VISIBLE;
        } else {
          return RecordVersionVisibility::INVISIBLE;
        }
      }
      // sub-case2: this record version isn't owned by any transaction
    } else {
      if (committed_version && !invalidated_version) {
        return RecordVersionVisibility::VISIBLE;
      } else {
        return RecordVersionVisibility::INVISIBLE;
      }
    }
  }
}

int TransactionContext::read_traverse_version_chain(
    RecordVersionHead &version_head, bool read_own, RecordLocation &rec_loc) {
  int retry_time = 0;
  int ret = FULGUR_RETRY;
  while (ret == FULGUR_RETRY && retry_time < 5) {
    if (read_own) {
      ret = mvto_read_vchain_own(version_head, rec_loc);
    } else {
      ret = mvto_read_unown_vchain(version_head, rec_loc);
    }
  }

  if (ret == FULGUR_RETRY) ret = FULGUR_FAIL;
  return ret;
}

int TransactionContext::get_transaction_status() {
  if (should_abort_)
    return FULGUR_TRANSACTION_ABORT;
  else
    return FULGUR_SUCCESS;
}

int TransactionContext::commit() {
  // do something
  // TODO
  if (read_only_) {
    reset();
    return FULGUR_SUCCESS;
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

    rloc_header->set_begin_timestamp(transaction_id_);
    rloc_header->set_end_timestamp(MAX_TIMESTAMP);
    rloc_header->set_transaction_id(INVALID_TRANSACTION_ID);
  }

  // then reset status
  reset();
  return FULGUR_SUCCESS;
}

void abort() {}

void set_read_only() { read_only_ = true; }

//===================private member funcitons============================
int TransactionContext::mvto_read_vchain_unown(RecordVersionHead &version_head,
                                               RecordLocation &rec_loc) {
  RecordHeader *version_iter = version_head.ptr_->get_record_header();
  while (version_iter != nullptr) {
    version_iter->latch_.lock();
    // begin_ts_ is immutable in version chain,
    // so we can find the first version that satisfy
    // version_iter->begin_ts_ <= transaction_id_
    if (transaction_id_ < version_iter->begin_ts_) {
      version_iter->latch_.unlock();
      version_iter = version_iter->older_->get_record_header();
      continue;
    }

    // At this point, we've got the first version that
    // satisfy version_iter->begin_ts_ <= transaction_id_

    // if it's a stable old version
    if (version_iter->end_ts_ != MAX_TIMESTAMP &&
        transaction_id_ <= version_iter->end_ts_) {
      version_iter->latch_.unlock();
      rec_loc.record_ = reinterpret_cast<Record *>(version_iter);
      // no need to set last_read_ts_ for an old version
      return FULGUR_SUCCESS;
    } else {
      // if it's the latest version
      version_iter->latch_.lock();
      // Got a free latest version, we can always read it
      if (version_iter->txn_id_ == INVALID_TRANSACTION_ID) {
        update_last_read_ts_if_need(version_iter);
        version_iter->latch_.unlock();
        rec_loc.record_ = reinterpret_cast<Record *>(version_iter);
        return FULGUR_SUCCESS;
        // A temporary latest version, but not stable
      } else if (version_iter->txn_id_ != INVALID_TRANSACTION_ID) {
        if (version_iter->txn_id_ < transaction_id_) {
          // A younger transaction is holding the version
          version_iter->latch_.unlock();
          return FULGUR_RETRY;
        } else if (transaction_id_ < version_iter->txn_id_) {
          // An older transaction is holding the version
          update_last_read_ts_if_need(version_iter);
          version_iter->latch_.unlock();
          rec_loc.record_ = reinterpret_cast<Record *>(version_iter);
          return FULGUR_SUCCESS;
        } else if (transaction_id_ == version_iter->txn_id_) {
          // FIXME: read from read_own_set ?
          version_iter->latch_.unlock();
          rec_loc.record_ = reinterpret_cast<Record *>(version_iter);
          return FULGUR_SUCCESS;
        }
      }
    }

    // panic: should not reach here
    assert(false);
  }

  // No valid version
  return FULGUR_FAIL;
}

int TransactionContext::mvto_read_vchain_own(RecordVersionHead &version_head,
                                             RecordLocation &rec_loc) {
  RecordHeader *version_iter = version_head.ptr_->get_record_header();
  version_iter->latch_.lock();
  // begin_ts_ is immutable in version chain,
  if (transaction_id_ < version_iter->begin_ts_) {
    version_iter->latch_.unlock();
    return FULGUR_FAIL;
  }

  // a deleted version
  if (version_iter->end_ts_ == 0) {
    version_iter->latch_.unlock();
    return FULGUR_FAIL;
  }
  // not the latest version anymore
  else if (version_iter->end_ts_ < transaction_id_) {
    version_iter->latch_.unlock();
    return FULGUR_RETRY;
    // still the latest version, and free
  } else if (version_iter->txn_id_ == INVALID_TRANSACTION_ID) {
    if (transaction_id_ < version_iter->last_read_ts_) {
      version_iter->latch_.unlock();
      return FULGUR_FAIL;
    } else {
      update_last_read_ts_if_need(version_iter);
      version_iter->latch_.unlock();
      rec_loc.record_ = reinterpret_cast<Record *>(version_iter);
      return FULGUR_SUCCESS;
    }
    // latest version, but not free
  } else if (version_iter->txn_id_ != INVALID_TRANSACTION_ID) {
    if (version_iter->txn_id_ < transaction_id_) {
      version_iter->latch_.unlock();
      return FULGUR_RETRY;
    } else if (transaction_id_ < version_iter->txn_id_) {
      version_iter->latch_.unlock();
      return FULGUR_FAIL;
    } else if (transaction_id_ == version_iter->txn_id_) {
      version_iter->latch_.unlock();
      rec_loc.record_ = reinterpret_cast<Record *>(version_iter);
      return FULGUR_SUCCESS;
    }
  }
  // panic: should not reach here
  assert(false);
  return FULGUR_FAIL;
}

/**
 *@brief caller must hold the latch of record header
 */
void TransactionContext::update_last_read_ts_if_need(
    RecordHeader *record_header) {
  if (record_header->last_read_ts_ < transaction_id_)
    record_header->last_read_ts_ = transaction_id_;
}

int TransactionContext::apply_read_own(RecordLocation &rec_loc) {
  RecordHeader *rec_header = rec_loc.get_record_header();
  // Case 1: current transaction is not the owner,
  //         we need to Retry or Fail
  if (!owned_by_me(rec_header)) {
    // Sub case 1: Now, someone has the ownership
    if (!owned_by_nobody(rec_header)) {
      uint64_t record_txn_id = rec_header->get_transaction_id();
      // sub sub case 1:
      // A later transaction has owned the version to write,
      // current transaction should abort.
      if (transaction_id_ < record_txn_id) {
        return FULGUR_FAIL;
        // sub sub case 2:
        // An earlier transaction has owned the version to write,
        // current transaction still has chance to wait for the
        // earlier transaction to commit and own its created version.
      } else {
        return FULGUR_RETRY;
      }
      // Sub case 2: nobody has ownership of this version,
      //             so, we can try to own it.
    } else {
      bool ret = get_version_ownership(rec_header, transaction_id_);
      // Get the ownership successfully, add it to read_own_set
      if (ret == true) {
        add_to_read_own_set(rec_loc);
        rec_header->latch_.lock();
        rec_header->last_read_ts_ = transaction_id_;
        rec_header->latch_.unlock();
        return true;
        // Another transaction got the ownership, we have to retry
        // or abort
      } else {
        // The "Another transaction" is older, we have to abort
        if (rec_header->get_transaction_id() > transaction_id_)
          return FULGUR_FAIL;
        // The "Another transaction" is younger, we still have chance
        else
          return FULGUR_RETRY;
      }
    }
    // Case 2: current transaction is the owner, just return true;
  } else {
    return FULGUR_SUCCESS;
  }
}

int TransactionContext::apply_read_not_own(RecordLocation &rec_loc) {
  RecordHeader *rec_header = rec_loc.get_record_header();
  rec_header->latch_.lock();
  if (transaction_id_ <= rec_header->end_ts_) {
    if (rec_header->last_read_ts_ < transaction_id_)
      rec_header->last_read_ts_ = transaction_id_;
    rec_header->latch_.unlock();
    return FULGUR_SUCCESS;
  } else {
    rec_header->latch_.unlock();
    return FULGUR_RETRY;
  }
}

/**
 *@brief
 *  if current version has not been owned and is the newest version,
 *  we can get the ownership
 */
bool TransactionContext::get_version_ownership(RecordHeader *header,
                                               uint64_t transaction_id) {
  header->latch_.lock();
  if (header->txn_id_ != INVALID_TRANSACTION_ID ||
      header->end_ts_ != MAX_TIMESTAMP) {
    header->latch_.unlock();
    return false;
  } else {
    header->txn_id_ = transaction_id;
    header->latch_.unlock();
    return true;
  }
}

bool TransactionContext::owned_by_me(RecordHeader *header) {
  if (header->get_transaction_id() == transaction_id_)
    return true;
  else
    return false;
}

bool TransactionContext::owned_by_nobody(RecordHeader *header) {
  if (header->get_transaction_id() == INVALID_TRANSACTION_ID)
    return true;
  else
    return false;
}

void TransactionContext::reset() {}

void TransactionContext::add_to_insert_set(Record *record) {
  txn_insert_set_.insert(record);
}

void TransactionContext::add_to_update_set(Record *record) {
  txn_update_set_.insert(record);
}

void TransactionContext::add_to_delete_set(Record *record) {
  txn_delete_set_.insert(record);
}

void TransactionContext::add_to_read_own_set(Record *record) {
  txn_read_own_set_.insert(record);
}

bool TransactionContext::in_update_set(Record *record) {
  if (txn_update_set_.find(record) != txn_update_set_.end())
    return true;
  else
    return false;
}

}  // namespace fulgurdb
