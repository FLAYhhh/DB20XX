#include "transaction.h"
#include <cstdint>
#include <exception>
#include "data_types.h"
#include "message_logger.h"
#include "record.h"
#include "return_status.h"
#include "table.h"
#include "thread_context.h"
#include "version_chain.h"
namespace fulgurdb {

//======================public member function=========================
bool TransactionContext::on_going() { return started_; }

void TransactionContext::begin_transaction(uint64_t thread_id) {
  transaction_id_ = GlocalEpochManager::enter_epoch(thread_id);
  epoch_id_ = transaction_id_ >> 32;
  thread_id_ = thread_id;
  started_ = true;
}

void TransactionContext::mvto_insert(Record *record, Table *table,
                                     ThreadContext *thd_ctx) {
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
    table->vchain_head_allocators_[writer_idx] =
        table->alloc_vchain_head_block();
  }

  vchain_head->set_latest_record(record);
  record->set_vchain_head(vchain_head);
  record->set_transaction_id(transaction_id_);
  record->set_last_read_timestamp(transaction_id_);
  // add_to_insert_set(record);
  add_to_modify_set(record);

  // We need to insert uncommited record to index,
  // so that subsequent queries in the same transaction
  // can find it from index
  table->insert_record_to_index(vchain_head, *ti);
}

// similar to mvto_update
int TransactionContext::mvto_delete(Record *record, Table *table,
                                    ThreadContext *thd_ctx) {
  if (record->get_transaction_id() == transaction_id_) {
    // the record have been inserted or updated by current transaction
    if (record->get_begin_timestamp() == MAX_TIMESTAMP) {
      record->set_end_timestamp(MIN_TIMESTAMP);
      return FULGUR_SUCCESS;
    } else {
      Record *new_record = nullptr;
      int status = table->alloc_record(new_record, thd_ctx);
      if (status != FULGUR_SUCCESS) return status;

      new_record->set_end_timestamp(MIN_TIMESTAMP);

      record->set_newer_version(new_record);
      new_record->set_older_version(record);
      new_record->set_vchain_head(record->get_vchain_head());
      new_record->set_transaction_id(transaction_id_);
      // add_to_delete_set(new_record);
      // add_to_modify_set(record);
      return FULGUR_SUCCESS;
    }

  } else {
    // Panic: assume update always own the record (no blind write)
    assert(false);
    return FULGUR_FAIL;
  }
}

// old_record may come from:
// 1. Table scan
// 2. Index search
int TransactionContext::mvto_update(Record *old_record, char *new_mysql_record,
                                    Table *table, ThreadContext *thd_ctx) {
  // current transaction already has the ownership of old record version
  // this happens in two conditios:
  // 1. ownership is got by read_own(table scan or index read)
  // 2  ownership is got by last update operation in the same transaction
  if (old_record->get_transaction_id() == transaction_id_) {
    // current transaction have updated the record
    if (old_record->get_begin_timestamp() == MAX_TIMESTAMP) {
      old_record->load_data_from_mysql(new_mysql_record, table->schema_);
      return FULGUR_SUCCESS;
    } else {
      Record *new_record = nullptr;
      int status = table->alloc_record(new_record, thd_ctx);
      if (status != FULGUR_SUCCESS) return status;

      new_record->load_data_from_mysql(new_mysql_record, table->schema_);

      old_record->set_newer_version(new_record);
      new_record->set_older_version(old_record);
      new_record->set_vchain_head(old_record->get_vchain_head());
      new_record->set_transaction_id(transaction_id_);
      // add_to_update_set(old_record);
      // add_to_modify_set(old_record);
      return FULGUR_SUCCESS;
    }
  } else {
    // Panic: assume update always own the record (no blind write)
    assert(false);
    return FULGUR_FAIL;
  }
}

/**
 *@brief
 *  mvto_read() and get_visibility() are used for table scan
 *  without index
 */
int TransactionContext::mvto_read_single_version(Record *record,
                                                 bool read_own) {
  LOG_TRACE("transaction_id_:%lu, begin_ts_:%lu, end_ts_:%lu", transaction_id_,
            record->get_begin_timestamp(), record->get_end_timestamp());
  if (read_own) {
    return mvto_read_single_own(record);
  } else
    return mvto_read_single_unown(record);
}

int TransactionContext::mvto_get_visibility(Record *record) {
  uint64_t record_begin_ts = record->get_begin_timestamp();
  uint64_t record_end_ts = record->get_end_timestamp();
  uint64_t record_txn_id = record->get_transaction_id();
  bool ownership = (transaction_id_ == record_txn_id);

  bool committed_version = (transaction_id_ >= record_begin_ts);
  bool invalidated_version = (transaction_id_ >= record_end_ts);

  if (ownership == true) {
    // condition: current transaction holds this record version
    // sub-case1: this is a newly inserted/updated record version
    if (record_begin_ts == MAX_TIMESTAMP && record_end_ts == MAX_TIMESTAMP) {
      return RecordVersionVisibility::VISIBLE;
    } else if (record_end_ts == INVALID_TIMESTAMP) {
      return RecordVersionVisibility::DELETED;
    } else {
      return RecordVersionVisibility::INVISIBLE;
    }
  } else {
    if (committed_version && !invalidated_version) {
      return RecordVersionVisibility::VISIBLE;
    } else {
      return RecordVersionVisibility::INVISIBLE;
    }
  }
}

int TransactionContext::mvto_read_version_chain(VersionChainHead &vchain_head,
                                                bool read_own,
                                                Record *&record) {
  int retry_time = 0;
  int ret = FULGUR_RETRY;
  while (ret == FULGUR_RETRY && retry_time < 1) {
    // TODO: sleep
    retry_time++;
    if (read_own) {
      ret = mvto_read_vchain_own(vchain_head, record);
    } else {
      ret = mvto_read_vchain_unown(vchain_head, record);
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
  // TODO: Log Module should persist modify set at this time
  // Because once we set begin_ts_, the record is visible to other transaction
  for (auto record : txn_modify_set_) {
    // Update & delete operation
    Record *new_version = record->get_newer_version();
    if (new_version != nullptr) {
      record->set_end_timestamp(transaction_id_);
      VersionChainHead *vchain_head = record->get_vchain_head();
      vchain_head->set_latest_record(new_version);
      // FIXME:BUG, assertion failed
      assert(new_version->get_begin_timestamp() ==
             MAX_TIMESTAMP);  // assert it's an uncommitted version
      new_version->set_begin_timestamp(transaction_id_);
    }
    // Insert operation
    if (record->get_begin_timestamp() == MAX_TIMESTAMP)
      record->set_begin_timestamp(transaction_id_);

    // TODO: add memory fence
    // release txn_id_ without lock is safe, because there is only one owner.
    record->set_transaction_id(INVALID_TRANSACTION_ID);
    if (new_version) new_version->set_transaction_id(INVALID_TRANSACTION_ID);
  }

  // then reset status
  LOG_TRACE("Transaction:%lu commit", transaction_id_);
  reset();
  return FULGUR_SUCCESS;
}

void TransactionContext::set_abort() { should_abort_ = true; }

void TransactionContext::abort() {
  for (auto record : txn_modify_set_) {
    Record *new_version = record->get_newer_version();
    if (new_version != nullptr) {
      Record *new_version = record->get_newer_version();
      new_version->set_end_timestamp(MIN_TIMESTAMP);
      record->set_newer_version(nullptr);
    }

    if (record->get_begin_timestamp() == MAX_TIMESTAMP) {
      record->set_end_timestamp(MIN_TIMESTAMP);
    }

    // TODO: add memory fence
    record->set_transaction_id(INVALID_TRANSACTION_ID);
    if (new_version) new_version->set_transaction_id(INVALID_TRANSACTION_ID);
  }

  LOG_TRACE("Transaction:%lu abort", transaction_id_);
  reset();
}

//===================private member funcitons============================
int TransactionContext::mvto_read_vchain_unown(VersionChainHead &vchain_head,
                                               Record *&record) {
  Record *version_iter = vchain_head.latest_record_;
  while (version_iter != nullptr) {
    // only need to lock the latest version, because old versions are stable
    if (version_iter == vchain_head.latest_record_) version_iter->lock_header();
    // begin_ts_ is immutable in version chain,
    // so we can find the first version that satisfy
    // version_iter->begin_ts_ <= transaction_id_
    if (transaction_id_ == version_iter->get_transaction_id()) {
      version_iter->unlock_header();
      if (version_iter->get_end_timestamp() != MIN_TIMESTAMP) {
        record = version_iter;
        return FULGUR_SUCCESS;
      } else {
        LOG_DEBUG("a deleted version");
        return FULGUR_FAIL;
      }
    } else if (transaction_id_ < version_iter->get_begin_timestamp()) {
      if (version_iter == vchain_head.latest_record_)
        version_iter->unlock_header();
      version_iter = version_iter->get_older_version();
      continue;
    }

    // At this point, we've got the first version that
    // satisfy version_iter->begin_ts_ <= transaction_id_

    // if it's a stable old visible version
    if (version_iter->get_end_timestamp() != MAX_TIMESTAMP &&
        transaction_id_ <= version_iter->get_end_timestamp()) {
      record = version_iter;
      return FULGUR_SUCCESS;
    } else if (version_iter->get_end_timestamp() == MIN_TIMESTAMP) {
      if (version_iter == vchain_head.latest_record_)
        version_iter->unlock_header();
      record = nullptr;
      LOG_DEBUG("meet a deleted version");
      return FULGUR_FAIL;
    } else {
      // if it's the latest version
      // Got a free latest version, we can always read it
      if (version_iter->get_transaction_id() == INVALID_TRANSACTION_ID) {
        update_last_read_ts_if_need(version_iter);
        version_iter->unlock_header();
        record = version_iter;
        return FULGUR_SUCCESS;
        // A temporary latest version, but not stable
      } else if (version_iter->get_transaction_id() != INVALID_TRANSACTION_ID) {
        if (version_iter->get_transaction_id() < transaction_id_) {
          // A older transaction is holding the version
          LOG_DEBUG("an older transaction[txn_id_:%lu] is owning the version",
                    version_iter->get_transaction_id());
          version_iter->unlock_header();
          Record *possible_newer_version = version_iter->get_newer_version();
          if (possible_newer_version &&
              possible_newer_version->get_end_timestamp() == MIN_TIMESTAMP) {
            // the older transaction wants to delete this version
            // LOG_DEBUG("an older deleter is owning the version");
            return FULGUR_FAIL;
          } else {
            // LOG_DEBUG("an older writer is owning the version");
            //  the younger transaction wants to update this version
            return FULGUR_RETRY;
          }
        } else if (transaction_id_ < version_iter->get_transaction_id()) {
          // An older transaction is holding the version
          update_last_read_ts_if_need(version_iter);
          version_iter->unlock_header();
          record = version_iter;
          return FULGUR_SUCCESS;
        } else if (transaction_id_ == version_iter->get_transaction_id()) {
          // Always read from version list head;
          // and always put the updated version ahead of the version chain;
          // so that we can always get the updated version.
          version_iter->unlock_header();
          if (version_iter->get_newer_version() != nullptr) {
            record = version_iter->get_newer_version();
          } else {
            record = version_iter;
          }
          return FULGUR_SUCCESS;
        }
      }
    }

    // panic: should not reach here
    assert(false);
  }

  // No valid version
  LOG_DEBUG("Transaction:%lu: older than all versions in the version chain",
            transaction_id_);
  return FULGUR_FAIL;
}

int TransactionContext::mvto_read_vchain_own(VersionChainHead &vchain_head,
                                             Record *&record) {
  Record *version_iter = vchain_head.latest_record_;
  version_iter->lock_header();
  // a commited version, but not visible
  if (version_iter->get_begin_timestamp() != MAX_TIMESTAMP &&
      transaction_id_ < version_iter->get_begin_timestamp()) {
    LOG_DEBUG(
        "Latest version is not visible, transaction_id_:%lu, begin_ts_:%lu",
        transaction_id_, version_iter->get_begin_timestamp());
    version_iter->unlock_header();
    return FULGUR_FAIL;
  } else if (version_iter->get_end_timestamp() == MIN_TIMESTAMP) {
    // a deleted version
    LOG_DEBUG("Latest version is a delete version, cannot own");
    version_iter->unlock_header();
    return FULGUR_FAIL;
  } else if (version_iter->get_end_timestamp() < transaction_id_) {
    // not the latest version anymore
    LOG_DEBUG(
        "not the latest version anymore, need retry. transaction_id_:%lu, "
        "end_ts_:%lu",
        transaction_id_, version_iter->get_end_timestamp());
    version_iter->unlock_header();
    return FULGUR_RETRY;
  } else if (version_iter->get_transaction_id() == INVALID_TRANSACTION_ID) {
    // still the latest version, and free
    if (transaction_id_ < version_iter->get_last_read_timestamp()) {
      LOG_ERROR(
          "Latest version has been read by newer transaction, cannot own. "
          "transaction_id_:%lu, last_read_ts_:%lu",
          transaction_id_, version_iter->get_last_read_timestamp());
      version_iter->unlock_header();
      return FULGUR_FAIL;
    } else {
      version_iter->set_transaction_id(transaction_id_);
      update_last_read_ts_if_need(version_iter);
      version_iter->unlock_header();
      record = version_iter;
      add_to_modify_set(record);
      return FULGUR_SUCCESS;
    }
    // latest version, but not free
  } else if (version_iter->get_transaction_id() != INVALID_TRANSACTION_ID) {
    if (version_iter->get_transaction_id() < transaction_id_) {
      LOG_DEBUG(
          "Transaction[%lu]: latest version is owned by older "
          "transaction[txn_id_:%lu], cannot own, retry",
          transaction_id_, version_iter->get_transaction_id());
      version_iter->unlock_header();
      return FULGUR_RETRY;
    } else if (transaction_id_ < version_iter->get_transaction_id()) {
      LOG_DEBUG(
          "Transaction[%lu]: latest version is owned by newer "
          "transaction[%lu], cannot own, fail",
          transaction_id_, version_iter->get_transaction_id());
      version_iter->unlock_header();
      return FULGUR_FAIL;
    } else if (transaction_id_ == version_iter->get_transaction_id()) {
      version_iter->unlock_header();
      if (version_iter->get_newer_version() != nullptr) {
        record = version_iter->get_newer_version();
      } else {
        record = version_iter;
      }
      return FULGUR_SUCCESS;
    }
  }
  // panic: should not reach here
  assert(false);
  return FULGUR_FAIL;
}

/**
 * @brief
 *   return values
 *     retval1 FULGUR_SUCCESS: visible and hold the ownership
 *     retval2 FULGUR_INVISIBLE_VERSION: invisible
 *     retval3 FULGUR_ABORT: visible but cannot hold the ownership
 *     retval4 FULGUR_RETRY: visible, still have chance to hold the ownership
 */
int TransactionContext::mvto_read_single_own(Record *record) {
  // visibility judge standards are different for owned and unowned versions

  // visible situation 1: old version with proper [begin, end) timestamp
  // read old version do not need to hold the lock
  if (record->get_end_timestamp() != MAX_TIMESTAMP &&
      record->get_begin_timestamp() <= transaction_id_ &&
      transaction_id_ <= record->get_end_timestamp()) {
    LOG_DEBUG("visible but can not hold ownership of an old version");
    return FULGUR_ABORT;
  }

  // visible situation 2: latest version with proper [bengin, end) timestamp
  //                      and proper txn_id_
  // need to hold lock to modify last_read_ts_
  record->lock_header();
  // an owned version
  if (record->get_transaction_id() != INVALID_TRANSACTION_ID) {
    if (record->get_transaction_id() == transaction_id_) {
      if (record->get_newer_version() == nullptr) {
        record->unlock_header();
        return FULGUR_SUCCESS;
      } else {
        LOG_DEBUG("meet a covered version");
        record->unlock_header();
        return FULGUR_INVISIBLE_VERSION;
      }
    } else if (record->get_transaction_id() < transaction_id_) {
      LOG_DEBUG(
          "Transaction[%lu]: this version is owned by older transaction[%lu], "
          "should retry",
          transaction_id_, record->get_transaction_id());
      record->unlock_header();
      return FULGUR_RETRY;
    } else if (transaction_id_ < record->get_transaction_id()) {
      if (record->get_begin_timestamp() <= transaction_id_) {
        LOG_DEBUG(
            "Transaction[%lu]: a newer version[%lu] have hold the ownership, "
            "assume it can commit, we "
            "should abort",
            transaction_id_, record->get_transaction_id());
        record->unlock_header();
        return FULGUR_ABORT;
      } else {
        record->unlock_header();
        return FULGUR_INVISIBLE_VERSION;
      }
    }
  } else {
    // a unowned version
    if (record->get_begin_timestamp() <= transaction_id_ &&
        transaction_id_ < record->get_end_timestamp()) {
      if (record->get_end_timestamp() == MAX_TIMESTAMP) {
        record->set_transaction_id(transaction_id_);
        record->unlock_header();
        add_to_modify_set(record);
        return FULGUR_SUCCESS;
      } else {
        LOG_DEBUG("can not own an old version");
        record->unlock_header();
        return FULGUR_ABORT;
      }
    } else {
      LOG_DEBUG("meet a invisible version");
      record->unlock_header();
      return FULGUR_INVISIBLE_VERSION;
    }
  }

  assert(false);
  return FULGUR_ABORT;
}

/**
 * @brief
 *   return values
 *     retval1 FULGUR_SUCCESS: visible
 *     retval2 FULGUR_INVISIBLE_VERSION: invisible
 *
 */
int TransactionContext::mvto_read_single_unown(Record *record) {
  // visibility judge standards are different for owned and unowned versions

  // visible situation 1: old version with proper [begin, end) timestamp
  // read old version do not need to hold the lock
  if (record->get_end_timestamp() != MAX_TIMESTAMP &&
      record->get_begin_timestamp() <= transaction_id_ &&
      transaction_id_ <= record->get_end_timestamp()) {
    return FULGUR_SUCCESS;
  }

  // visible situation 2: latest version with proper [bengin, end) timestamp
  //                      and proper txn_id_
  // need to hold lock to modify last_read_ts_
  record->lock_header();
  // an owned version
  if (record->get_transaction_id() != INVALID_TRANSACTION_ID) {
    if (record->get_transaction_id() == transaction_id_) {
      if (record->get_newer_version() == nullptr) {
        record->unlock_header();
        return FULGUR_SUCCESS;
      } else {
        LOG_DEBUG("meet a covered version");
        record->unlock_header();
        return FULGUR_INVISIBLE_VERSION;
      }
    } else if (record->get_transaction_id() < transaction_id_) {
      LOG_DEBUG(
          "this version is owned by older transaction, should retry. older "
          "transaction_id_:%lu, current_transaction_id_:%lu",
          record->get_transaction_id(), transaction_id_);
      record->unlock_header();
      return FULGUR_RETRY;
    } else if (transaction_id_ < record->get_transaction_id()) {
      if (record->get_begin_timestamp() <= transaction_id_) {
        update_last_read_ts_if_need(record);
        record->unlock_header();
        return FULGUR_SUCCESS;
      } else {
        record->unlock_header();
        return FULGUR_INVISIBLE_VERSION;
      }
    }
  } else {
    // a unowned version
    if (record->get_begin_timestamp() <= transaction_id_ &&
        transaction_id_ < record->get_end_timestamp()) {
      update_last_read_ts_if_need(record);
      record->unlock_header();
      return FULGUR_SUCCESS;
    } else {
      LOG_DEBUG("meet a invisible version");
      record->unlock_header();
      return FULGUR_INVISIBLE_VERSION;
    }
  }

  // nerver reach here
  assert(false);
  return FULGUR_ABORT;
}

/**
 *@brief caller must hold the latch of record header
 */
void TransactionContext::update_last_read_ts_if_need(Record *record) {
  if (record->get_last_read_timestamp() < transaction_id_)
    record->set_last_read_timestamp(transaction_id_);
}

/*
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

void TransactionContext::add_to_ins_del_set(Record *record) {
  txn_ins_del_set_.insert(record);
}
*/

void TransactionContext::add_to_modify_set(Record *record) {
  txn_modify_set_.insert(record);
}

void TransactionContext::reset() {
  transaction_id_ = INVALID_TRANSACTION_ID;
  epoch_id_ = 0;
  thread_id_ = 0;
  started_ = false;
  should_abort_ = false;
  txn_modify_set_.clear();
}

}  // namespace fulgurdb
