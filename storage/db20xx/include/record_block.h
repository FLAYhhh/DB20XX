#pragma once
#include "record.h"
#include "return_status.h"

namespace db20xx {
class TableScanCursor;
class RecordBlock {
  friend class Table;

 public:
  bool is_last_record(const Record *record);
  int alloc_record(Record *&record);
  void get_record(TableScanCursor *scan_cursor);

 private:
  uint32_t block_id_ = 0;
  uint32_t record_length_ = 0;  // include header + payload
  uint32_t record_capacity_ = 0;
  std::atomic<uint32_t> valid_record_num_{0};
  char records_data_[0];
};

}  // end of namespace db20xx
