#pragma once
#include "record.h"
#include "record_location.h"
#include "return_status.h"

namespace fulgurdb {

class RecordBlockHeader {

};

class RecordBlock {
friend class Table;
public:
  bool is_last_record(const RecordLocation &rec_loc) {
    if (rec_loc.idx_in_block_ == record_capacity_ - 1)
      return true;
    else
      return false;
  }

  int alloc_record(RecordLocation &rec_loc) {
    uint32_t offset = valid_record_num_.fetch_add(1,
                          std::memory_order_relaxed);
    if (offset >= record_capacity_)
      return FULGUR_BLOCK_FULL;

    rec_loc.block_id_ = block_id_;
    rec_loc.idx_in_block_ = offset;
    rec_loc.record_ = reinterpret_cast<Record*>(
         records_data_ + offset * record_length_);

    rec_loc.record_->init_header();

    return FULGUR_SUCCESS;
  }

  int get_record(RecordLocation &rec_loc) {
    if (rec_loc.idx_in_block_ >= valid_record_num_)
      return FULGUR_INVALID_RECORD_LOCATION;

    rec_loc.record_ = reinterpret_cast<Record*>(records_data_ +
              rec_loc.idx_in_block_ * record_length_);

    return FULGUR_SUCCESS;
  }
private:
  uint32_t block_id_ = 0;
  uint32_t record_length_ = 0; // include header + payload
  uint32_t record_capacity_ = 0;
  std::atomic<uint32_t> valid_record_num_ = 0;
  char records_data_[0];
};

} // end of namespace fulgurdb
