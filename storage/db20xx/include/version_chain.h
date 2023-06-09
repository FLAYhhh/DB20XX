#pragma once
#include <sys/types.h>
#include "return_status.h"
#include "utils.h"

namespace db20xx {
class Record;
class TableScanCursor;

class VersionChainHead {
 public:
  char *get_latest_record_payload();
  void set_latest_record(Record *latest_record);
  void init();

 public:
  Record *latest_record_;
};

class VersionChainHeadBlock {
  friend class Table;

 public:
  int alloc_vchain_head(VersionChainHead *&vchain_head);
  bool is_last_vchain_head(VersionChainHead *vchain_head);
  VersionChainHead *get_vchain_head(TableScanCursor *scan_cursor);

 public:
  static const uint32_t ENTRY_CAPACITY = 1024;

 private:
  uint32_t block_id_ = 0;
  std::atomic<uint32_t> valid_entry_num_ = 0;
  VersionChainHead entries_[ENTRY_CAPACITY];
};

}  // namespace db20xx
