#include "record_block.h"
#include "table.h"
namespace db20xx {
bool RecordBlock::is_last_record(const Record *record) {
  if ((const char *)record ==
      records_data_ + record_length_ * (record_capacity_ - 1))
    return true;
  else
    return false;
}

int RecordBlock::alloc_record(Record *&record) {
  uint32_t offset = valid_record_num_.fetch_add(1, std::memory_order_relaxed);
  if (offset >= record_capacity_) return FULGUR_BLOCK_FULL;
  record = reinterpret_cast<Record *>(records_data_ + offset * record_length_);
  record->init();

  return FULGUR_SUCCESS;
}

void RecordBlock::get_record(TableScanCursor *scan_cursor) {
  assert(block_id_ == scan_cursor->block_id_);
  scan_cursor->record_ = reinterpret_cast<Record *>(
      records_data_ + scan_cursor->idx_in_block_ * record_length_);
}
}  // end of namespace db20xx
