#include "transaction.h"
#include <cstdint>
#include <exception>
#include "data_types.h"
#include "record.h"
#include "record_location.h"
#include "return_status.h"
#include "table.h"
#include "thread_local.h"
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

  record->set_vchain_head(vchain_head);
  record->set_transaction_id(transaction_id_);
  record->set_last_read_timestamp(transaction_id_);
  //add_to_insert_set(record);
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
      //add_to_delete_set(new_record);
      add_to_modify_set(record);
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
      add_to_update_set(old_record);
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
  if (read_own)
    return mvto_read_single_own(record);
  else
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
  while (ret == FULGUR_RETRY && retry_time < 5) {
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
  // Insert:
  // for insert records, we need to make it visiable,
  // free latch & set begin/end timestamp
  for (auto &record : txn_insert_set_) {
    record->set_begin_timestamp(transaction_id_);
    // we should have set end_ts_ in mvto_insert()
    // record->set_end_timestamp(MAX_TIMESTAMP);

    // TODO: add memory fence here
    record->set_transaction_id(INVALID_TRANSACTION_ID);
  }
  // Update:
  // txn_update_set_ contains old records
  for (auto &record : txn_update_set_) {
    VersionChainHead *vchain_head = record->get_vchain_head();
    Record *new_version = record->get_newer_version();

    vchain_head->set_latest_record(new_version);
    // set end_ts_ of old record at committed,
    // because other transaction can get visibility info by
    // txn_id_ of old record
    record->set_end_timestamp(transaction_id_);
    new_version->set_begin_timestamp(transaction_id_);

    // TODO:add memory fence here
    record->set_transaction_id(INVALID_TRANSACTION_ID);
    new_version->set_transaction_id(INVALID_TRANSACTION_ID);
  }

  for (auto record : txn_delete_set_) {
    VersionChainHead *vchain_head = record->get_vchain_head();
    Record *new_version = record->get_newer_version();

    vchain_head->set_latest_record(new_version);
    record->set_end_timestamp(transaction_id_);
    // we should have set end_ts_ in mvto_delete()
    // new_version->set_end_timestamp(MIN_TIMESTAMP);
    new_version->set_begin_timestamp(transaction_id_);

    // TODO: add memory fence here;
    record->set_transaction_id(INVALID_TRANSACTION_ID);
    new_version->set_transaction_id(INVALID_TRANSACTION_ID);
  }

  for (auto record : txn_ins_del_set_) {
    // we should have set end_ts_ in mvto_delete()
    // record->set_end_timestamp(MIN_TIMESTAMP);
    record->set_transaction_id(INVALID_TRANSACTION_ID);
  }

  // Read Own:
  // for read own records, we just need to free the latch
  for (auto &record : txn_read_own_set_) {
    record->set_transaction_id(INVALID_TRANSACTION_ID);
  }

  // then reset status
  reset();
  return FULGUR_SUCCESS;
}

void TransactionContext::abort() {
  // FIXME: Now, leave the insert version in index, and make it invisible
  for (auto &record : txn_insert_set_) {
    // FIXME: delete from index
    record->set_end_timestamp(MIN_TIMESTAMP);
    record->set_transaction_id(INVALID_TRANSACTION_ID);
  }

  // leave ins-del record as it is.
  for (auto &record : txn_ins_del_set_) {
    record->set_transaction_id(INVALID_TRANSACTION_ID);
  }

  // restore original latest version
  for (auto &record : txn_delete_set_) {
    Record *new_version = record->get_newer_version();
    new_version->set_end_timestamp(MIN_TIMESTAMP);
    record->set_newer_version(nullptr);

    record->set_transaction_id(INVALID_TRANSACTION_ID);
    new_version->set_transaction_id(INVALID_TRANSACTION_ID);
  }

  for (auto &record : txn_read_own_set_) {
    record->set_transaction_id(INVALID_TRANSACTION_ID);
  }
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
    if (transaction_id_ < version_iter->get_begin_timestamp()) {
      version_iter->unlock_header();
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
      record = nullptr;
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
          // A younger transaction is holding the version
          version_iter->unlock_header();
          Record *possible_newer_version = version_iter->get_newer_version();
          if (possible_newer_version &&
              possible_newer_version->get_end_timestamp() == MIN_TIMESTAMP) {
            // the younger transaction wants to delete this version
            return FULGUR_FAIL;
          } else {
            // the younger transaction wants to update this version
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
  return FULGUR_FAIL;
}

int TransactionContext::mvto_read_vchain_own(VersionChainHead &vchain_head,
                                             Record *&record) {
  Record *version_iter = vchain_head.latest_record_;
  version_iter->lock_header();
  // a commited version, but not visible
  if (version_iter->get_begin_timestamp() != MAX_TIMESTAMP &&
      transaction_id_ < version_iter->get_begin_timestamp()) {
    version_iter->unlock_header();
    return FULGUR_FAIL;
  } else if (version_iter->get_end_timestamp() == MIN_TIMESTAMP) {
    // a deleted version
    version_iter->unlock_header();
    return FULGUR_FAIL;
  } else if (version_iter->get_end_timestamp() < transaction_id_) {
    // not the latest version anymore
    version_iter->unlock_header();
    return FULGUR_RETRY;
  } else if (version_iter->get_transaction_id() == INVALID_TRANSACTION_ID) {
    // still the latest version, and free
    if (transaction_id_ < version_iter->get_last_read_timestamp()) {
      version_iter->unlock_header();
      return FULGUR_FAIL;
    } else {
      update_last_read_ts_if_need(version_iter);
      version_iter->unlock_header();
      record = version_iter;
      return FULGUR_SUCCESS;
    }
    // latest version, but not free
  } else if (version_iter->get_transaction_id() != INVALID_TRANSACTION_ID) {
    if (version_iter->get_transaction_id() < transaction_id_) {
      version_iter->unlock_header();
      return FULGUR_RETRY;
    } else if (transaction_id_ < version_iter->get_transaction_id()) {
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

int TransactionContext::mvto_read_single_own(Record *record) {
  // Can not own an old version
  if (record->get_end_timestamp() != MAX_TIMESTAMP) {
    return FULGUR_FAIL;
  }

  if (record->get_transaction_id() != INVALID_TRANSACTION_ID) {
    // Latest and owned by someone
    if (record->get_transaction_id() == transaction_id_) {
      // owned by current transaction
      if (record->get_newer_version() == nullptr)
        return FULGUR_SUCCESS;
      else
        return FULGUR_FAIL;
    } else if (record->get_transaction_id() < transaction_id_) {
      // owned by others
      return FULGUR_RETRY;
    } else {
      return FULGUR_FAIL;
    }
  } else {
    // Latest and not owned by anyone
    record->lock_header();
    if (record->get_transaction_id() == INVALID_TRANSACTION_ID &&
        record->get_end_timestamp() == MAX_TIMESTAMP) {
      record->set_transaction_id(transaction_id_);
      record->unlock_header();
      return FULGUR_SUCCESS;
    } else {
      record->unlock_header();
      return FULGUR_FAIL;
    }
  }
}

// FIXME
int TransactionContext::mvto_read_single_unown(Record *record) {
  record->lock_header();
  if (record->get_begin_timestamp() <= transaction_id_ &&
      transaction_id_ < record->get_end_timestamp()) {
    if (record->get_transaction_id() == INVALID_TRANSACTION_ID) {
      update_last_read_ts_if_need(record);
      record->unlock_header();
      return FULGUR_SUCCESS;
    } else if (record->get_transaction_id() < transaction_id_) {
      record->unlock_header();
      return FULGUR_RETRY;
    } else if (transaction_id_ < record->get_transaction_id()) {
      record->unlock_header();
      return FULGUR_FAIL;
    } else if (transaction_id_ == record->get_transaction_id()) {
      if (record->get_newer_version() == nullptr) {
        record->unlock_header();
        return FULGUR_SUCCESS;
      } else {
        record->unlock_header();
        return FULGUR_FAIL;
      }
    }
  } else if (record->get_transaction_id() < transaction_id_) {
    record->unlock_header();
    return FULGUR_RETRY;
  } else {
    record->unlock_header();
    return FULGUR_FAIL;
  }

  // nerver reach here
  return FULGUR_FAIL;
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
}

}  // namespace fulgurdb
