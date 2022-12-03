#pragma once
#include <atomic>
#include "data_types.h"
#include "index_indirection.h"
#include "transaction.h"
#include "utils.h"
#include "./schema.h"
#include "./record_location.h"
#include "./index.h"
#include "./thread_local.h"
#include "./record_block.h"
#include "./return_status.h"
#include "./cuckoo_map.h"

namespace fulgurdb {

struct TableScanCursor {
  uint32_t block_id_;
  uint32_t idx_in_block_;
};

class Table {
public:
  Table(const std::string &table_name, Schema &schema):
     table_name_(table_name), schema_(schema) {
     init_block_writers();
     init_record_ptr_allocators();
  }

  /**
  @brief get table schema
  */
  const Schema& get_schema() const {return schema_;}


  /**
   *@brief
   *  insert an invisiable record to table
   *@args
   *  arg3 rloc[output]: record location of the new record
   */
  int insert_record_from_mysql(char *mysql_record,
                               ThreadContext *thd_ctx,
                               RecordLocation &rloc) {
    int status = FULGUR_SUCCESS;

    status = alloc_record(rloc, thd_ctx);
    if (status != FULGUR_SUCCESS)
      return status;

    rloc.load_data_from_mysql(mysql_record, schema_);

    // Transaction module: set record header & add new record to rw set
    TransactionContext *txn_ctx = thd_ctx->get_transaction_context();
    txn_ctx->apply_insert(rloc);

    return status;
  }

  /**
  @brief
    Table scan without index
    User should advance scan_cursor mannually before call this function.
    This function will correct scan_cursor if idx_in_block_ exceed limit 
  */
  int table_scan_get(RecordLocation &scan_cursor, bool read_own,
                     ThreadContext *thd_ctx) {
    //first time in the block
    if (scan_cursor.record_ == nullptr) {
      table_scan_cached_block_ = get_block(scan_cursor.block_id_);
    }

    // jump to next useful block
    while (scan_cursor.idx_in_block_ ==
        table_scan_cached_block_->valid_record_num_) {
      // have reached the end of current block, jump to next
      scan_cursor.block_id_ += 1;
      scan_cursor.idx_in_block_ = 0;
      table_scan_cached_block_ = get_block(scan_cursor.block_id_);

      if (scan_cursor.block_id_ >= next_block_id_)
        return FULGUR_END_OF_TABLE;
    }

    int status = FULGUR_SUCCESS;
    status = table_scan_cached_block_->get_record(scan_cursor);
    assert(status == FULGUR_SUCCESS);

    // Transaction Module:
    // First, do visibility judge, we should jump invisible records
    // Second, for visible records, there are two types of read operation:
    //   1. read own (typically in update operation, have two stage: read + write)
    //   2. read (typically in select operation)

    TransactionContext *txn_ctx = thd_ctx->get_transaction_context();
    auto visibility = txn_ctx->get_visibility(scan_cursor);
    if (visibility == RecordVersionVisibility::VISIBLE) {
      return txn_ctx->apply_read(scan_cursor, read_own);
    } else {
      return FULGUR_GET_INVISIBLE_VERSION;
    }
  }

  /**
  @brief
    build an index on given key information
    KeyInfo contains:
       0. table schema
       1. columns id that consist of key
       2. key length
  */
  void build_index(const KeyInfo &keyinfo, threadinfo &ti) {
    MasstreeIndex *index = new MasstreeIndex(keyinfo);
    index->initialize(ti);
    indexes_.push_back(index);
  }


  /**
  @brief
    insert record location to index
  */
  void insert_record_to_index(uint32_t idx, RecordPtr *p_record_ptr,
                         threadinfo &ti) {
    std::unique_ptr<Key> key = indexes_[idx]->build_key(
                           p_record_ptr->get_record_payload());
    indexes_[idx]->put(*key, p_record_ptr, ti);
  }

  void insert_record_to_index(RecordPtr *p_record_ptr, threadinfo &ti) {
    for (size_t i = 0; i < indexes_.size(); i++) {
      insert_record_to_index(i, p_record_ptr, ti);
    }
  }

  /**
  @brief
    given a index number and its corresponding key, get the record.

  @return values
    @retval true: key exists
    @retval false: key does not exist
  */
  bool get_record_from_index(uint32_t idx, const Key &key,
              RecordLocation &rec_loc, ThreadContext &thd_ctx,
              bool read_own) {
    RecordPtr *p_record_ptr = nullptr;
    bool found = indexes_[idx]->get(key, p_record_ptr, *thd_ctx.ti_);
    if (!found)
      return false;

    // Traverse the version chain to find a valid version
    TransactionContext *txn_ctx = thd_ctx.get_transaction_context();
    int ret = txn_ctx->read_traverse_version_chain(
              *p_record_ptr, read_own, rec_loc);
    if (ret == FULGUR_SUCCESS)
      return true;
    else
      return false;
  }

  bool index_scan_range_first(uint32_t idx, const Key &key,
             RecordLocation &rec_loc, bool emit_firstkey,
             scan_stack_type &scan_stack, ThreadContext &thd_ctx,
             bool read_own) const {
    RecordPtr *p_record_ptr = nullptr;
    scan_stack.reset();

    bool found = indexes_[idx]->scan_range_first(key, p_record_ptr,
                   emit_firstkey,
                   scan_stack,
                   *thd_ctx.ti_);
    if (!found)
      return false;

    // Traverse the version chain to find a valid version
    TransactionContext *txn_ctx = thd_ctx.get_transaction_context();
    int ret = txn_ctx->read_traverse_version_chain(
              *p_record_ptr, read_own, rec_loc);
    if (ret == FULGUR_SUCCESS) {
      return true;
    } else if (emit_firstkey) {
      return false;
    } else if (!emit_firstkey) {
      return index_scan_range_next(idx, rec_loc, scan_stack,
                                thd_ctx, read_own);
    }

    return false;
  }

  int index_scan_range_next(uint32_t idx, RecordLocation &rec_loc,
                       scan_stack_type &scan_stack,
                       ThreadContext &thd_ctx,
                       bool read_own) const {
    RecordPtr *p_record_ptr = nullptr;
    bool found = indexes_[idx]->scan_range_next(p_record_ptr,
                    scan_stack, *thd_ctx.ti_);
    if (!found)
      return false;

    // Traverse the version chain to find a valid version
    TransactionContext *txn_ctx = thd_ctx.get_transaction_context();
    int ret = txn_ctx->read_traverse_version_chain(
              *p_record_ptr, read_own, rec_loc);
    if (ret == FULGUR_SUCCESS) {
      return true;
    }

    else if (ret == FULGUR_FAIL)
      return index_scan_range_next(idx, rec_loc, scan_stack,
                                thd_ctx, read_own);
    else {
      // panic
      assert(false);
      return false;
    }
  }

  bool index_rscan_range_first(uint32_t idx, const Key &key,
             RecordLocation &rec_loc, bool emit_firstkey,
             scan_stack_type &scan_stack, ThreadContext &thd_ctx,
             bool read_own) const {
    RecordPtr *p_record_ptr = nullptr;
    scan_stack.reset();

    bool found = indexes_[idx]->rscan_range_first(key, p_record_ptr,
                   emit_firstkey,
                   scan_stack,
                   *thd_ctx.ti_);
    if (!found)
      return false;

    // Traverse the version chain to find a valid version
    TransactionContext *txn_ctx = thd_ctx.get_transaction_context();
    int ret = txn_ctx->read_traverse_version_chain(
              *p_record_ptr, read_own, rec_loc);
    if (ret == FULGUR_SUCCESS) {
      return true;
    } else if (emit_firstkey) {
      return false;
    } else if (!emit_firstkey) {
      return index_scan_range_next(idx, rec_loc, scan_stack,
                                thd_ctx, read_own);
    }

    return false;
  }

  bool index_rscan_range_next(uint32_t idx, RecordLocation &rec_loc,
                       scan_stack_type &scan_stack,
                       ThreadContext &thd_ctx,
                       bool read_own) const {
    RecordPtr *p_record_ptr = nullptr;
    bool found = indexes_[idx]->rscan_range_next(p_record_ptr,
                    scan_stack, *thd_ctx.ti_);
    if (!found)
      return false;

    // Traverse the version chain to find a valid version
    TransactionContext *txn_ctx = thd_ctx.get_transaction_context();
    int ret = txn_ctx->read_traverse_version_chain(
              *p_record_ptr, read_own, rec_loc);
    if (ret == FULGUR_SUCCESS) {
      return true;
    }

    else if (ret == FULGUR_FAIL)
      return index_scan_range_next(idx, rec_loc, scan_stack,
                                thd_ctx, read_own);
    else {
      // panic
      assert(false);
      return false;
    }
  }

private:
  /**
  @brief
    1. Allocate and initialize an invisible record in table store;
    2. Allocate an index indirection layer entry for this record,
       and insert it to index;
    3. Caller should fill record data to empty record slot.
  @return
    location to the record
  */
  int alloc_record(RecordLocation &rec_loc, ThreadContext *thd_ctx) {
    uint32_t writer_idx = thd_ctx->get_thread_id()
                             % PARALLEL_WRITER_NUM;
    RecordBlock *block = nullptr;
    int status = FULGUR_SUCCESS;
    do {
      block = record_writers_[writer_idx];
      status = block->alloc_record(rec_loc);
    } while (status != FULGUR_SUCCESS);

    // 仅有一个线程获得了block的最后一条记录,
    // 由该线程负责更新这个位置的record_writer,
    // 这种方法确保了只有一个线程会负责替换writer,
    // 避免了并发修改
    if (block->is_last_record(rec_loc)) {
      record_writers_[writer_idx] = alloc_block();
    }

    // insert the record to index
    threadinfo *ti = thd_ctx->get_threadinfo();
    RecordPtrBlock *record_ptr_block = nullptr;
    RecordPtr *p_record_ptr = nullptr; //pointer to RecordPtr
    status = FULGUR_SUCCESS;
    do {
      record_ptr_block = record_ptr_allocators_[writer_idx];
      status = record_ptr_block->alloc_record_ptr(p_record_ptr);
    } while (status != FULGUR_SUCCESS);

    if (record_ptr_block->is_last_record(p_record_ptr)) {
      record_ptr_allocators_[writer_idx] = alloc_record_ptr_block();
    }

    // @note: now, record data has not been filled,
    //        but it does not matter.
    insert_record_to_index(p_record_ptr, *ti);

    rec_loc.record_->set_indirection(p_record_ptr);

    return status;
  }

  //FIXME: use per-thread allocator
  RecordBlock *alloc_block() {
    uint32_t complete_record_length = sizeof(RecordHeader)
                      + schema_.get_record_data_length();
    uint32_t block_size = sizeof(RecordBlock) +
                  records_in_block_ * complete_record_length;
    RecordBlock *block = (RecordBlock*)malloc(block_size);
    block = new (block) RecordBlock;
    block->record_length_ = complete_record_length;
    block->record_capacity_ = records_in_block_;
    block->block_id_ = next_block_id_.fetch_add(1, std::memory_order_relaxed);

    return block;
  }

  //FIXME: use per-thread allocator
  RecordPtrBlock *alloc_record_ptr_block() {
    RecordPtrBlock *block =  new RecordPtrBlock();
    block->block_id_ = next_record_ptr_block_id.fetch_add(1,
                   std::memory_order_relaxed);
    return block;
  }

  /**
  @brief add a block to table store
  */
  void add_block(RecordBlock *block) {
    record_blocks_.Upsert(block->block_id_, block);
  }

  void add_record_ptr_block(RecordPtrBlock *block) {
    record_ptr_blocks_.Upsert(block->block_id_, block);
  }

  /**
  @brief given a block id, get the block address of the table store
  */
  RecordBlock* get_block(uint32_t block_id) {
    RecordBlock *block;
    record_blocks_.Find(block_id, block);
    return block;
  }

  /**
  @brief
    initialize block writers, each thread delegate its write operation
    to a block writer with index = thread_id % PARALLEL_WRITER_NUM.
  */
  void init_block_writers() {
    for (size_t i = 0; i < PARALLEL_WRITER_NUM; i++) {
      record_writers_[i] = alloc_block();
      add_block(record_writers_[i]);
    }
  }

  void init_record_ptr_allocators() {
    for (size_t i = 0; i < PARALLEL_WRITER_NUM; i++) {
      record_ptr_allocators_[i] = alloc_record_ptr_block();
      add_record_ptr_block(record_ptr_allocators_[i]);
    }
  }


private:
  // static members
  static const uint32_t PARALLEL_WRITER_NUM = 16;

private:
  // table metadata
  std::string table_name_;
  Schema schema_;

  // table storage
  std::atomic<uint32_t> next_block_id_;
  const uint32_t DEFAULT_RECORDS_PER_BLOCK = 1024;
  uint32_t records_in_block_ = DEFAULT_RECORDS_PER_BLOCK;
  CuckooMap<uint32_t, RecordBlock*> record_blocks_;

  std::array<RecordBlock*, PARALLEL_WRITER_NUM> record_writers_;


  // table scan
  RecordBlock *table_scan_cached_block_;

  // index
  std::vector<MasstreeIndex *> indexes_;

  std::atomic<uint32_t> next_record_ptr_block_id;
  CuckooMap<uint32_t, RecordPtrBlock*> record_ptr_blocks_;
  std::array<RecordPtrBlock*, PARALLEL_WRITER_NUM> record_ptr_allocators_;
};
}
