#pragma once
#include <atomic>
#include "index_indirection.h"
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
  @brief
    allocate a record space.
    user should fill record data to empty record slot.

  @return
    location to the record
  */
  RecordLocation alloc_record(ThreadLocal *thd_ctx) {
    uint32_t writer_idx = thd_ctx->get_thread_id()
                             % PARALLEL_WRITER_NUM;

    RecordLocation rec_loc;
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

    insert_record_to_index(p_record_ptr, *ti);

    rec_loc.record_->set_indirection(p_record_ptr);

    return rec_loc;
  }

  /**
  @brief
    Table scan without index
    User should advance scan_cursor mannually before call this function.
    This function will correct scan_cursor if idx_in_block_ exceed limit 
  */
  int table_scan_get(RecordLocation &scan_cursor) {
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

    return FULGUR_SUCCESS;
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
              RecordPtr *&p_record_ptr, threadinfo &ti) {
    return indexes_[idx]->get(key, p_record_ptr, ti);
  }

  bool index_scan_range_first(uint32_t idx, const Key &key,
             RecordPtr *&p_record_ptr, bool emit_firstkey,
             ThreadLocal &thd_ctx) const {
    thd_ctx.reset_masstree_scan_stack();
    return indexes_[idx]->scan_range_first(key, p_record_ptr,
                   emit_firstkey,
                   thd_ctx.masstree_scan_stack_,
                   *thd_ctx.ti_);
  }

  bool index_scan_range_next(uint32_t idx, RecordPtr *&p_record_ptr,
                       ThreadLocal &thd_ctx) const {
    return indexes_[idx]->scan_range_next(p_record_ptr,
                    thd_ctx.masstree_scan_stack_,
                    *thd_ctx.ti_);
  }

  bool index_rscan_range_first(uint32_t idx, const Key &key,
             RecordPtr *&p_record_ptr, bool emit_firstkey,
             ThreadLocal &thd_ctx) const {
    thd_ctx.reset_masstree_scan_stack();
    return indexes_[idx]->rscan_range_first(key, p_record_ptr,
                   emit_firstkey,
                   thd_ctx.masstree_scan_stack_,
                   *thd_ctx.ti_);
  }

  bool index_rscan_range_next(uint32_t idx, RecordPtr *&p_record_ptr,
                       ThreadLocal &thd_ctx) const {
    return indexes_[idx]->rscan_range_next(p_record_ptr,
                    thd_ctx.masstree_scan_stack_,
                    *thd_ctx.ti_);
  }

private:
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
