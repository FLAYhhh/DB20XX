#include "table.h"
#include <cassert>
#include <cstdint>
#include <memory>
#include "data_types.h"
#include "index.h"
#include "message_logger.h"
#include "return_status.h"
#include "transaction.h"
#include "version_chain.h"

namespace fulgurdb {
Table::Table(const std::string &table_name, Schema &schema)
    : table_name_(table_name), schema_(schema) {
  init_record_allocators();
  init_vchain_head_allocators();
}

/**
@brief get table schema
*/
const Schema &Table::get_schema() const { return schema_; }

//======================Insert operation==============================
/**
 *@brief
 *  insert an invisiable record to table
 *@args
 *  arg3 rloc[output]: record location of the new record
 */
int Table::insert_record_from_mysql(char *mysql_record,
                                    ThreadContext *thd_ctx) {
  int status = FULGUR_SUCCESS;
  Record *record = nullptr;
  TransactionContext *txn_ctx = thd_ctx->get_transaction_context();
  VersionChainHead *vchain_head = nullptr;

  // check primary key existance
  if (indexes_.size() > 0) {
    Key key;
    indexes_[0]->build_key(mysql_record, key);
    int ret = get_record_from_index(0, key, record, *thd_ctx, false);
    indexes_[0]->release_key(key);

    if (ret == FULGUR_KEY_NOT_EXIST) {
      // do nothing, pass checking
    } else if (ret == FULGUR_DELETED_VERSION) {
      // The only condition that we can do insertion on an exist version chain
      // Insert a new version after a newest deleted version
      record->lock_header();
      if (record->get_transaction_id() == INVALID_TRANSACTION_ID &&
          record->get_newer_version() == nullptr) {
        record->set_transaction_id(txn_ctx->transaction_id_);
        vchain_head = record->get_vchain_head();
        txn_ctx->add_to_modify_set(record);
        record->unlock_header();
      } else {
        record->unlock_header();
        txn_ctx->set_abort();
        return FULGUR_ABORT;
      }
    } else {
      return FULGUR_KEY_EXIST;
    }
  }

  status = alloc_record(record, thd_ctx);
  if (status != FULGUR_SUCCESS) {
    LOG_DEBUG("alloc_record failed");
    return status;
  }

  record->load_data_from_mysql(mysql_record, schema_);
  txn_ctx->mvto_insert(record, vchain_head, this, thd_ctx);

  return status;
}
//=====================Update operation==============================
int Table::update_record_from_mysql(Record *old_record, char *new_mysql_record,
                                    ThreadContext *thd_ctx) {
  TransactionContext *txn_ctx = thd_ctx->get_transaction_context();
  int ret = txn_ctx->mvto_update(old_record, new_mysql_record, this, thd_ctx);
  assert(ret == FULGUR_SUCCESS);

  return ret;
}

//=====================Delete operation==============================
int Table::delete_record(Record *record, ThreadContext *thd_ctx) {
  TransactionContext *txn_ctx = thd_ctx->get_transaction_context();
  int ret = txn_ctx->mvto_delete(record, this, thd_ctx);
  assert(ret == FULGUR_SUCCESS);

  return ret;
}

//=====================Table scan=====================================
/**
@brief
  Table scan without index
  User should advance scan_cursor mannually before call this function.
  This function will correct scan_cursor if idx_in_block_ exceed limit 
*/
int Table::table_scan_get(TableScanCursor &scan_cursor, bool read_own,
                          ThreadContext *thd_ctx) {
  if (scan_cursor.record_ == nullptr) {
    table_scan_cached_block_ = get_vchain_head_block(scan_cursor.block_id_);
  }
  assert(table_scan_cached_block_ != nullptr);

  // jump to next useful block
  while (scan_cursor.idx_in_block_ ==
         table_scan_cached_block_->valid_entry_num_.load()) {
    // have reached the end of current block, jump to next
    scan_cursor.block_id_ += 1;
    scan_cursor.idx_in_block_ = 0;

    if (scan_cursor.block_id_ >= next_vchain_head_block_id_) {
      LOG_DEBUG("table scan end");
      return FULGUR_END_OF_TABLE;
    }
    table_scan_cached_block_ = get_vchain_head_block(scan_cursor.block_id_);
    assert(table_scan_cached_block_ != nullptr);
    LOG_DEBUG(
        "swith to next block. scan_cursor.block_id_: %u, "
        "scan_cursor.idx_in_block_: %u, cached_block_id: %u, "
        "cached_block.valid_entry_num_: %u",
        scan_cursor.block_id_, scan_cursor.idx_in_block_,
        table_scan_cached_block_->block_id_,
        table_scan_cached_block_->valid_entry_num_.load());
  }

  VersionChainHead *vchain_head =
      table_scan_cached_block_->get_vchain_head(&scan_cursor);

  TransactionContext *txn_ctx = thd_ctx->get_transaction_context();
  int ret = txn_ctx->mvto_read_version_chain(*vchain_head, read_own,
                                             scan_cursor.record_);
  if (ret == FULGUR_ABORT || ret == FULGUR_RETRY) {
    txn_ctx->set_abort();
  }

  if (ret == FULGUR_INVISIBLE_VERSION || ret == FULGUR_DELETED_VERSION) {
    LOG_TRACE("Table:%s, invisible version", table_name_.c_str());
  }

  // SUCCESS or INVISIBLE don't need to abort
  return ret;
}

//===================Index Operations===========================

/**
@brief
  build an index on given key information
  KeyInfo contains:
     0. table schema
     1. columns id that consist of key
     2. key length
*/
void Table::build_index(const KeyInfo &keyinfo, threadinfo &ti) {
  MasstreeIndex *index = new MasstreeIndex(keyinfo);
  index->initialize(ti);
  indexes_.push_back(index);
}

/**
@brief
  insert record location to index
*/
void Table::insert_record_to_index(uint32_t idx, VersionChainHead *vchain_head,
                                   threadinfo &ti) {
  Key key;
  indexes_[idx]->build_key(vchain_head->get_latest_record_payload(), key);
  indexes_[idx]->put(key, vchain_head, ti);
  indexes_[idx]->release_key(key);
}

void Table::insert_record_to_index(VersionChainHead *vchain_head,
                                   threadinfo &ti) {
  for (size_t i = 0; i < indexes_.size(); i++) {
    insert_record_to_index(i, vchain_head, ti);
  }
}

/**
@brief
  Index point read
@return values
  @retval true: key exists
  @retval false: key does not exist
*/
int Table::get_record_from_index(uint32_t idx, const Key &key, Record *&record,
                                  ThreadContext &thd_ctx, bool read_own) {
  VersionChainHead *vchain_head = nullptr;
  bool found = indexes_[idx]->get(key, vchain_head, *thd_ctx.ti_);
  if (!found) {
    //LOG_DEBUG("do not find in index");
    return FULGUR_KEY_NOT_EXIST;
  }

  // Traverse the version chain to find a valid version
  TransactionContext *txn_ctx = thd_ctx.get_transaction_context();
  int ret = txn_ctx->mvto_read_version_chain(*vchain_head, read_own, record);
  if (ret == FULGUR_ABORT) {
    txn_ctx->set_abort();
  }

  return ret;
}

int Table::index_scan_range_first(uint32_t idx, const Key &key,
                                   Record *&record, bool emit_firstkey,
                                   scan_stack_type &scan_stack,
                                   ThreadContext &thd_ctx,
                                   bool read_own) const {
  VersionChainHead *vchain_head = nullptr;
  scan_stack.reset();

  bool found = indexes_[idx]->scan_range_first(key, vchain_head, emit_firstkey,
                                               scan_stack, *thd_ctx.ti_);
  if (!found) return FULGUR_KEY_NOT_EXIST;

  // Traverse the version chain to find a valid version
  TransactionContext *txn_ctx = thd_ctx.get_transaction_context();
  int ret = txn_ctx->mvto_read_version_chain(*vchain_head, read_own, record);
  if (ret == FULGUR_ABORT) {
    txn_ctx->set_abort();
    return ret;
  }

  if (ret == FULGUR_SUCCESS) {
    return true;
  } else if (emit_firstkey) {
    return false;
  } else {
    return index_scan_range_next(idx, record, scan_stack, thd_ctx, read_own);
  }
}

int Table::index_scan_range_next(uint32_t idx, Record *&record,
                                  scan_stack_type &scan_stack,
                                  ThreadContext &thd_ctx, bool read_own) const {
  VersionChainHead *vchain_head = nullptr;
  bool found =
      indexes_[idx]->scan_range_next(vchain_head, scan_stack, *thd_ctx.ti_);
  if (!found) return FULGUR_INDEX_RANGE_END;

  // Traverse the version chain to find a valid version
  TransactionContext *txn_ctx = thd_ctx.get_transaction_context();
  int ret = txn_ctx->mvto_read_version_chain(*vchain_head, read_own, record);
  if (ret == FULGUR_ABORT) {
    txn_ctx->set_abort();
    return ret;
  }
  if (ret == FULGUR_SUCCESS) {
    return ret;
  }

  else if (ret == FULGUR_DELETED_VERSION || ret == FULGUR_INVISIBLE_VERSION)
    return index_scan_range_next(idx, record, scan_stack, thd_ctx, read_own);
  else {
    // panic
    assert(false);
    return ret;
  }
}

int Table::index_rscan_range_first(uint32_t idx, const Key &key,
                                    Record *&record, bool emit_firstkey,
                                    scan_stack_type &scan_stack,
                                    ThreadContext &thd_ctx,
                                    bool read_own) const {
  VersionChainHead *vchain_head = nullptr;
  scan_stack.reset();

  bool found = indexes_[idx]->rscan_range_first(key, vchain_head, emit_firstkey,
                                                scan_stack, *thd_ctx.ti_);
  if (!found) return FULGUR_KEY_NOT_EXIST;

  // Traverse the version chain to find a valid version
  TransactionContext *txn_ctx = thd_ctx.get_transaction_context();
  int ret = txn_ctx->mvto_read_version_chain(*vchain_head, read_own, record);
  if (ret == FULGUR_ABORT) {
    txn_ctx->set_abort();
    return ret;
  }

  if (ret == FULGUR_SUCCESS) {
    return ret;
  } else if (emit_firstkey) {
    return ret;
  } else {
    assert(emit_firstkey == false);
    return index_scan_range_next(idx, record, scan_stack, thd_ctx, read_own);
  }
}

int Table::index_rscan_range_next(uint32_t idx, Record *&record,
                                   scan_stack_type &scan_stack,
                                   ThreadContext &thd_ctx,
                                   bool read_own) const {
  VersionChainHead *vchain_head = nullptr;
  bool found =
      indexes_[idx]->rscan_range_next(vchain_head, scan_stack, *thd_ctx.ti_);
  if (!found) return FULGUR_INDEX_RANGE_END;

  // Traverse the version chain to find a valid version
  TransactionContext *txn_ctx = thd_ctx.get_transaction_context();
  int ret = txn_ctx->mvto_read_version_chain(*vchain_head, read_own, record);
  if (ret == FULGUR_ABORT) {
    txn_ctx->set_abort();
    return ret;
  }

  if (ret == FULGUR_SUCCESS) {
    return ret;
  }

  else if (ret == FULGUR_DELETED_VERSION || ret == FULGUR_INVISIBLE_VERSION)
    return index_scan_range_next(idx, record, scan_stack, thd_ctx, read_own);
  else {
    // panic
    assert(false);
    return ret;
  }
}

int Table::index_prefix_key_search(uint32_t idx, const Key &key,
                                    Record *&record,
                                    scan_stack_type &scan_stack,
                                    ThreadContext &thd_ctx,
                                    bool read_own) const {
  VersionChainHead *vchain_head = nullptr;
  scan_stack.reset();

  // found=true means scan has not reached the end
  bool found = indexes_[idx]->scan_range_first(key, vchain_head, true,
                                               scan_stack, *thd_ctx.ti_);

  if (!found) return FULGUR_KEY_NOT_EXIST;

  Key current_key = scan_stack.get_current_key().full_string();
  if (current_key.less_than(key)) {
    return index_prefix_search_next(idx, key, record, scan_stack, thd_ctx, read_own);
  } else if (!current_key.has_prefix(key)) {
    return FULGUR_KEY_NOT_EXIST;
  }

  // Traverse the version chain to find a valid version
  TransactionContext *txn_ctx = thd_ctx.get_transaction_context();
  int ret = txn_ctx->mvto_read_version_chain(*vchain_head, read_own, record);
  if (ret == FULGUR_ABORT) {
    txn_ctx->set_abort();
    return FULGUR_ABORT;
  }

  if (ret == FULGUR_SUCCESS) {
    return FULGUR_SUCCESS;
  } else if (ret == FULGUR_INVISIBLE_VERSION || ret == FULGUR_DELETED_VERSION) {
    return index_prefix_search_next(idx, key, record, scan_stack, thd_ctx,
                                    read_own);
  } else {
    assert(false);
  }

  assert(false);
  return FULGUR_ABORT;
}

int Table::index_prefix_search_next(uint32_t idx, const Key &key,
                                     Record *&record,
                                     scan_stack_type &scan_stack,
                                     ThreadContext &thd_ctx,
                                     bool read_own) const {
  VersionChainHead *vchain_head = nullptr;

  // found=true means scan has not reached the end
  bool found =
      indexes_[idx]->scan_range_next(vchain_head, scan_stack, *thd_ctx.ti_);

  if (!found) return FULGUR_INDEX_RANGE_END;

  Key current_key = scan_stack.get_current_key().full_string();
  if (!current_key.has_prefix(key)) {
    return FULGUR_INDEX_RANGE_END;
  }

  // Traverse the version chain to find a valid version
  TransactionContext *txn_ctx = thd_ctx.get_transaction_context();
  int ret = txn_ctx->mvto_read_version_chain(*vchain_head, read_own, record);
  if (ret == FULGUR_ABORT) {
    txn_ctx->set_abort();
    return ret;
  } else if (ret == FULGUR_SUCCESS) {
    return ret;
  } else if (ret == FULGUR_INVISIBLE_VERSION || ret == FULGUR_DELETED_VERSION) {
    LOG_DEBUG("Transaction[%lu], read version chain fail, vchain_head:%p", txn_ctx->transaction_id_, vchain_head);
    return index_prefix_search_next(idx, key, record, scan_stack, thd_ctx, read_own);
  } else {
    assert(false);
  }
}

//========================private member
// functions=============================
/**
@brief
  1. Allocate and initialize an invisible record in table store;
  2. Allocate an index indirection layer entry for this record,
     and insert it to index;
  3. Caller should fill record data to empty record slot.
@return
  location to the record
*/
int Table::alloc_record(Record *&record, ThreadContext *thd_ctx) {
  uint32_t writer_idx = thd_ctx->get_thread_id() % PARALLEL_WRITER_NUM;
  RecordBlock *record_block = nullptr;
  int status = FULGUR_SUCCESS;

  // Step1: Alloc record
  do {
    record_block = record_allocators_[writer_idx];
    status = record_block->alloc_record(record);
  } while (status != FULGUR_SUCCESS);

  // 仅有一个线程获得了block的最后一条记录,
  // 由该线程负责更新这个位置的record_writer,
  // 这种方法确保了只有一个线程会负责替换writer,
  // 避免了并发修改
  if (record_block->is_last_record(record)) {
    record_allocators_[writer_idx] = alloc_record_block();
  }

  return status;
}

// FIXME: use per-thread allocator
RecordBlock *Table::alloc_record_block() {
  uint32_t complete_record_length =
      sizeof(RecordHeader) + schema_.get_record_data_length();
  uint32_t block_size =
      sizeof(RecordBlock) + records_in_block_ * complete_record_length;
  RecordBlock *block = (RecordBlock *)malloc(block_size);
  block = new (block) RecordBlock;
  block->record_length_ = complete_record_length;
  block->record_capacity_ = records_in_block_;
  block->block_id_ =
      next_record_block_id_.fetch_add(1, std::memory_order_relaxed);

  add_record_block(block);

  return block;
}

// FIXME: use per-thread allocator
VersionChainHeadBlock *Table::alloc_vchain_head_block() {
  VersionChainHeadBlock *block = new VersionChainHeadBlock();
  block->block_id_ =
      next_vchain_head_block_id_.fetch_add(1, std::memory_order_relaxed);

  add_vchain_head_block(block);
  return block;
}

/**
@brief add a block to table store
*/
void Table::add_record_block(RecordBlock *block) {
  // LOG_TRACE("RecordBlock block_id_: %u", block->block_id_);
  record_blocks_.Upsert(block->block_id_, block);
}

void Table::add_vchain_head_block(VersionChainHeadBlock *block) {
  // LOG_TRACE("VchainHeadBlock block_id_: %u", block->block_id_);
  vchain_head_blocks_.Upsert(block->block_id_, block);
}

/**
@brief given a block id, get the block address of the table store
*/
RecordBlock *Table::get_record_block(uint32_t block_id) {
  RecordBlock *block = nullptr;
  bool ret = record_blocks_.Find(block_id, block);
  assert(ret == true);
  (void)ret;
  return block;
}

VersionChainHeadBlock *Table::get_vchain_head_block(uint32_t block_id) {
  VersionChainHeadBlock *block = nullptr;
  bool ret = vchain_head_blocks_.Find(block_id, block);
  assert(ret == true);
  (void)ret;
  return block;
}

/**
@brief
  initialize block writers, each thread delegate its write operation
  to a block writer with index = thread_id % PARALLEL_WRITER_NUM.
*/
void Table::init_record_allocators() {
  for (size_t i = 0; i < PARALLEL_WRITER_NUM; i++) {
    record_allocators_[i] = alloc_record_block();
  }
}

void Table::init_vchain_head_allocators() {
  for (size_t i = 0; i < PARALLEL_WRITER_NUM; i++) {
    vchain_head_allocators_[i] = alloc_vchain_head_block();
  }
}

}  // namespace fulgurdb
