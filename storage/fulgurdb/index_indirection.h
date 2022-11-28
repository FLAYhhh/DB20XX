#pragma once
#include <sys/types.h>
#include "return_status.h"
#include "utils.h"

namespace fulgurdb {

class Record;
/**
 *@brief
 *  index value type is [RecordPtrWrapper *],
 *  so that, we don't need to modify index when update
 */
class RecordPtr {
public:
  char *get_record_payload();
public:
  Record *ptr_;
};


/**
 *@brief
 *  Memory allocation unit for Index indirection layer.
 *  similar to RecordBlock
 */
class RecordPtrBlock {
friend class Table;
public:
  int alloc_record_ptr(RecordPtr *&p_record_ptr) {
    uint32_t offset = valid_entry_num_.fetch_add(1,
                        std::memory_order_relaxed);
    if (offset >= RECORD_PTR_CAPACITY)
      return FULGUR_BLOCK_FULL;
    p_record_ptr = &entries_[offset];

    return FULGUR_SUCCESS;
  }

  bool is_last_record(RecordPtr *p_record_ptr) {
    if (p_record_ptr == &entries_[RECORD_PTR_CAPACITY - 1])
      return true;
    else
      return false;
  }
public:
  static const uint32_t RECORD_PTR_CAPACITY = 1024;
private:
  uint32_t block_id_ = 0;
  std::atomic<uint32_t> valid_entry_num_ = 0;
  RecordPtr entries_[RECORD_PTR_CAPACITY];
};

}
