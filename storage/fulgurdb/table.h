#pragma once
#include <sys/types.h>
#include <atomic>
#include <cstdint>
#include "cuckoo_map.h"
#include "data_types.h"
#include "index.h"
#include "record.h"
#include "record_block.h"
#include "return_status.h"
#include "schema.h"
#include "thread_context.h"
#include "transaction.h"
#include "utils.h"
#include "version_chain.h"

namespace fulgurdb {

class TableScanCursor {
  friend class Table;

 public:
  void reset() {
    block_id_ = 0;
    idx_in_block_ = 0;
    record_ = nullptr;
  }

  /**
  @brief
    used in sequential table scan, increase record location to next.
    we will correct the cursor in Table::table_scan_get().
  */
  void inc_cursor() { idx_in_block_ += 1; }

 public:
  uint32_t block_id_ = 0;
  uint32_t idx_in_block_ = 0;
  Record *record_ = nullptr;
};

class Table {
  friend class TransactionContext;

 public:
  Table(const std::string &table_name, Schema &schema);
  const Schema &get_schema() const;
  int insert_record_from_mysql(char *mysql_record, ThreadContext *thd_ctx);
  int update_record_from_mysql(Record *old_record, char *new_mysql_record,
                               ThreadContext *thd_ctx);
  int delete_record(Record *record, ThreadContext *thd_ctx);
  /**
  @brief
    Table scan without index
    User should advance scan_cursor mannually before call this function.
    This function will correct scan_cursor if idx_in_block_ exceed limit 
  */
  int table_scan_get(TableScanCursor &scan_cursor, bool read_own,
                     ThreadContext *thd_ctx);

  //=======================Index operations============================
  /**
  @brief
    build an index on given key information
    KeyInfo contains:
       0. table schema
       1. columns id that consist of key
       2. key length
  */
  void build_index(const KeyInfo &keyinfo, threadinfo &ti);

  /**
  @brief
    insert record location to index
  */
  void insert_record_to_index(uint32_t idx, VersionChainHead *vchain_head,
                              ThreadContext *thd_ctx);

  void insert_record_to_index(VersionChainHead *vchain_head, ThreadContext *thd_ctx);

  /**
  @brief
    given a index number and its corresponding key, get the record.
  */
  int get_record_from_index(uint32_t idx, const Key &key, Record *&record,
                             ThreadContext &thd_ctx, bool read_own);

  int index_scan_range_first(uint32_t idx, const Key &key, Record *&record,
                              bool emit_firstkey, scan_stack_type &scan_stack,
                              ThreadContext &thd_ctx, bool read_own) const;

  /**
   *@return values
   *  @retval true: Not the end
   *  @retval false: End of index
   */
  int index_scan_range_next(uint32_t idx, Record *&record,
                             scan_stack_type &scan_stack,
                             ThreadContext &thd_ctx, bool read_own) const;

  int index_rscan_range_first(uint32_t idx, const Key &key, Record *&record,
                               bool emit_firstkey, scan_stack_type &scan_stack,
                               ThreadContext &thd_ctx, bool read_own) const;

  int index_rscan_range_next(uint32_t idx, Record *&record,
                              scan_stack_type &scan_stack,
                              ThreadContext &thd_ctx, bool read_own) const;

  uint32_t get_key_length(uint32_t idx) {
    return indexes_[idx]->get_key_length();
  }

  int index_prefix_key_search(uint32_t idx, const Key &key, Record *&record,
                               scan_stack_type &scan_stack,
                               ThreadContext &thd_ctx, bool read_own) const;

  int index_prefix_search_next(uint32_t idx, const Key &key, Record *&record,
                                scan_stack_type &scan_stack,
                                ThreadContext &thd_ctx, bool read_own) const;

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
  int alloc_record(Record *&record, ThreadContext *thd_ctx);
  // FIXME: use per-thread allocator
  RecordBlock *alloc_record_block();
  // FIXME: use per-thread allocator
  VersionChainHeadBlock *alloc_vchain_head_block();
  void add_record_block(RecordBlock *block);
  void add_vchain_head_block(VersionChainHeadBlock *block);
  RecordBlock *get_record_block(uint32_t block_id);
  VersionChainHeadBlock *get_vchain_head_block(uint32_t block_id);

  /**
  @brief
    initialize block writers, each thread delegate its write operation
    to a block writer with index = thread_id % PARALLEL_WRITER_NUM.
  */
  void init_record_allocators();
  void init_vchain_head_allocators();

 private:
  // static members
  static const uint32_t PARALLEL_WRITER_NUM = 16;

 private:
  // table metadata
  std::string table_name_;
  Schema schema_;

  // table storage
  std::atomic<uint32_t> next_record_block_id_ = 0;
  const uint32_t DEFAULT_RECORDS_PER_BLOCK = 1024;
  uint32_t records_in_block_ = DEFAULT_RECORDS_PER_BLOCK;
  CuckooMap<uint32_t, RecordBlock *> record_blocks_;
  std::array<RecordBlock *, PARALLEL_WRITER_NUM> record_allocators_;

  // table scan
  VersionChainHeadBlock *table_scan_cached_block_;

  // index
  std::vector<MasstreeIndex *> indexes_;
  std::atomic<uint32_t> next_vchain_head_block_id_ = 0;
  CuckooMap<uint32_t, VersionChainHeadBlock *> vchain_head_blocks_;
  std::array<VersionChainHeadBlock *, PARALLEL_WRITER_NUM>
      vchain_head_allocators_;
};
}  // namespace fulgurdb
