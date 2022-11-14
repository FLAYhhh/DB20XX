#pragma once
#include "utils.h"
#include "./schema.h"
#include "./record_location.h"
#include "./index.h"
#include "./thread_local.h"

namespace fulgurdb {
class Table {
public:
  Table(const std::string &table_name, Schema &schema):
     table_name_(table_name), schema_(schema){}

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
  RecordLocation alloc_record() {
    char *data = (char *)malloc(schema_.total_size_);
    return RecordLocation(data);
  }


  /**
  @brief
    insert a record to table
  */
  void insert_record(const RecordLocation &rloc) {
    records_.push_back(rloc.data_);
  }


  /**
  @brief
    get record with sequence number in the table
  */
  char* get_record_data(uint32_t row_idx) const {
    if (row_idx >= records_.size())
      return nullptr;
    else
      return records_[row_idx];
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
  void insert_record_to_index(uint32_t idx, const RecordLocation &rloc,
                         threadinfo &ti) {
    std::unique_ptr<Key> key = indexes_[idx]->build_key(rloc.get_record());
    indexes_[idx]->put(*key, rloc, ti);
  }

  void insert_record_to_index(const RecordLocation &rloc, threadinfo &ti) {
    for (size_t i = 0; i < indexes_.size(); i++) {
      insert_record_to_index(i, rloc, ti);
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
              RecordLocation &rloc, threadinfo &ti) {
    return indexes_[idx]->get(key, rloc, ti);
  }

  bool index_scan_range_first(uint32_t idx, const Key &key,
             RecordLocation &rloc, bool emit_firstkey,
             ThreadLocal &thd_ctx) const {
    thd_ctx.reset_masstree_scan_stack();
    return indexes_[idx]->scan_range_first(key, rloc,
                   emit_firstkey,
                   thd_ctx.masstree_scan_stack_,
                   *thd_ctx.ti_);
  }

  bool index_scan_range_next(uint32_t idx, RecordLocation &rloc,
                       ThreadLocal &thd_ctx) const {
    return indexes_[idx]->scan_range_next(rloc,
                    thd_ctx.masstree_scan_stack_,
                    *thd_ctx.ti_);
  }

  bool index_rscan_range_first(uint32_t idx, const Key &key,
             RecordLocation &rloc, bool emit_firstkey,
             ThreadLocal &thd_ctx) const {
    thd_ctx.reset_masstree_scan_stack();
    return indexes_[idx]->rscan_range_first(key, rloc,
                   emit_firstkey,
                   thd_ctx.masstree_scan_stack_,
                   *thd_ctx.ti_);
  }

  bool index_rscan_range_next(uint32_t idx, RecordLocation &rloc,
                       ThreadLocal &thd_ctx) const {
    return indexes_[idx]->rscan_range_next(rloc,
                    thd_ctx.masstree_scan_stack_,
                    *thd_ctx.ti_);
  }

private:
  std::string table_name_;
  Schema schema_;
  std::vector<char *> records_;

  // index
  std::vector<MasstreeIndex *> indexes_;
};
}
