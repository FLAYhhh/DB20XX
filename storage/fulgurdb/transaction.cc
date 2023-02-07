#include "transaction.h"
#include <chrono>
#include <cstdint>
#include <exception>
#include <thread>
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

void TransactionContext::mvto_insert(Record *record, VersionChainHead *vchain_head, Table *table,
                                     ThreadContext *thd_ctx) {
  // Alloc version chain head & insert it to index
  uint32_t writer_idx = thd_ctx->get_thread_id() % Table::PARALLEL_WRITER_NUM;
  VersionChainHeadBlock *vchain_head_block = nullptr;
  int status = FULGUR_SUCCESS;

  if (vchain_head == nullptr) {
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
    table->insert_record_to_index(vchain_head, thd_ctx);
  } else {
    Record *deleted_version = vchain_head->latest_record_;
    deleted_version->set_newer_version(record);
    record->set_older_version(deleted_version);
    record->set_transaction_id(transaction_id_);
    record->set_vchain_head(vchain_head);
  }
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

int TransactionContext::mvto_read_version_chain(VersionChainHead &vchain_head,
                                                bool read_own,
                                                Record *&record) {
  int retry_time = 0;
  int ret = FULGUR_RETRY;
  while (ret == FULGUR_RETRY) {
    if (retry_time != 0)
      std::this_thread::sleep_for(std::chrono::microseconds(10));
    if (read_own) {
      ret = mvto_read_vchain_own(vchain_head, record);
    } else {
      ret = mvto_read_vchain_unown(vchain_head, record);
    }
    retry_time++;
  }

  if (ret == FULGUR_RETRY) ret = FULGUR_ABORT;
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
    // Update & delete & insert(on exist vchain) operation
    Record *new_version = record->get_newer_version();
    if (new_version != nullptr) {
      if (record->get_end_timestamp() != MIN_TIMESTAMP)
        record->set_end_timestamp(transaction_id_);
      VersionChainHead *vchain_head = record->get_vchain_head();
      // FIXME:BUG, assertion failed
      assert(new_version->get_begin_timestamp() ==
             MAX_TIMESTAMP);  // assert it's an uncommitted version
      new_version->set_begin_timestamp(transaction_id_);
      // TODO: add memory fence (make sure versions in vchain are committed)
      vchain_head->set_latest_record(new_version);
    }
    // Insert(create vchain) operation
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

    // insert(create new vchain)
    if (record->get_begin_timestamp() == MAX_TIMESTAMP) {
      record->set_begin_timestamp(MIN_TIMESTAMP);
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
    // Rewrite start
    // traverse to a visible version without lock
    if (transaction_id_ < version_iter->header_.begin_ts_) {
      version_iter = version_iter->header_.older_version_;
      continue;
    }

    // a stable old version, also need no lock
    if (version_iter->header_.end_ts_ != MAX_TIMESTAMP) {
      record = version_iter;
      if (version_iter->header_.end_ts_ == MIN_TIMESTAMP) {
        return FULGUR_DELETED_VERSION;
      } else if (transaction_id_ < version_iter->header_.end_ts_) {
        return FULGUR_SUCCESS;
      } else if ( transaction_id_ >= version_iter->header_.end_ts_) {
        return FULGUR_RETRY;
      } else {
        assert(false);
      }
    }

    // other cases: an latest version in previous check, (but may have become an old version at this point)

    // owned by current transaction
    if (version_iter->header_.txn_id_ == transaction_id_) {
      if (version_iter->header_.newer_version_) {
        version_iter = version_iter->header_.newer_version_;
        assert(version_iter->header_.txn_id_ == transaction_id_);
        assert(version_iter->header_.begin_ts_ == MAX_TIMESTAMP);
      }
      record = version_iter;
      if (version_iter->header_.end_ts_ == MIN_TIMESTAMP) {
        return FULGUR_DELETED_VERSION;
      } else {
        return FULGUR_SUCCESS;
      }
    }

    // owned by others or noone
    version_iter->lock_header();
    if (version_iter->header_.txn_id_ == INVALID_TRANSACTION_ID) {
      update_last_read_ts_if_need(version_iter);
      version_iter->unlock_header();
      record = version_iter;
      return FULGUR_SUCCESS;
    } else {
      // because giveup owership of record at commit time does not hold lock,
      // txn_id_ can be 0 here.
      if (version_iter->header_.txn_id_ < transaction_id_ &&
          version_iter->header_.txn_id_ != INVALID_TRANSACTION_ID) {
        version_iter->unlock_header();
        LOG_DEBUG("Transaction[%lu]: an older transaction[txn_id_:%lu] is owning the version",
                  transaction_id_, version_iter->get_transaction_id());
        record = version_iter;
        return FULGUR_RETRY;
      } else if (transaction_id_ < version_iter->header_.txn_id_) {
        update_last_read_ts_if_need(version_iter);
        version_iter->unlock_header();
        record = version_iter;
        return FULGUR_SUCCESS;
      }
    }

    LOG_ERROR("concurrent exception path");
    version_iter->unlock_header();
    return FULGUR_RETRY;
  }

  // No valid version
  LOG_DEBUG("Transaction:%lu: older than all versions in the version chain",
            transaction_id_);
  return FULGUR_INVISIBLE_VERSION;
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
    return FULGUR_ABORT;
  } else if (version_iter->get_end_timestamp() == MIN_TIMESTAMP) {
    // a deleted version
    LOG_DEBUG("Latest version is a delete version, cannot own");
    version_iter->unlock_header();
    return FULGUR_ABORT;
  } else if (version_iter->get_end_timestamp() < transaction_id_) {
    // not the latest version anymore
    LOG_DEBUG(
        "Transaction[%lu]:not the latest version anymore, need retry. end_ts_:%lu",
        transaction_id_, version_iter->get_end_timestamp());
    version_iter->unlock_header();
    return FULGUR_RETRY;
  } else if (version_iter->get_transaction_id() == INVALID_TRANSACTION_ID) {
    // still the latest version, and free
    if (transaction_id_ < version_iter->get_last_read_timestamp()) {
      LOG_DEBUG(
          "Transaction[%lu]:Latest version has been read by newer transaction, cannot own. last_read_ts_:%lu",
          transaction_id_, version_iter->get_last_read_timestamp());
      version_iter->unlock_header();
      return FULGUR_ABORT;
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
      return FULGUR_ABORT;
    } else {
      assert(transaction_id_ == version_iter->get_transaction_id());
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
  return FULGUR_RETRY;
}

/**
 *@brief caller must hold the latch of record header
 */
void TransactionContext::update_last_read_ts_if_need(Record *record) {
  if (record->get_last_read_timestamp() < transaction_id_)
    record->set_last_read_timestamp(transaction_id_);
}

void TransactionContext::reset() {
  transaction_id_ = INVALID_TRANSACTION_ID;
  epoch_id_ = 0;
  thread_id_ = 0;
  started_ = false;
  should_abort_ = false;
  txn_modify_set_.clear();
}

void TransactionContext::add_to_modify_set(Record *record) {
  txn_modify_set_.insert(record);
}

}  // namespace fulgurdb
