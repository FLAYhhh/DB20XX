#include "version_chain.h"
#include "record.h"
#include "table.h"

namespace db20xx {
char *VersionChainHead::get_latest_record_payload() {
  return latest_record_->get_payload();
}

void VersionChainHead::set_latest_record(Record *latest_record) {
  latest_record_ = latest_record;
}

void VersionChainHead::init() { latest_record_ = nullptr; }

int VersionChainHeadBlock::alloc_vchain_head(VersionChainHead *&vchain_head) {
  uint32_t offset = valid_entry_num_.fetch_add(1, std::memory_order_relaxed);
  // assert(valid_entry_num_.load() <= ENTRY_CAPACITY);
  // in multi-thread cases, valid_entry_num_ may exceed ENTRY_CAPACITY;
  if (offset >= ENTRY_CAPACITY) {
    vchain_head = nullptr;
    return DB20XX_BLOCK_FULL;
  } else {
    vchain_head = &entries_[offset];
    vchain_head->init();
    return DB20XX_SUCCESS;
  }
}

bool VersionChainHeadBlock::is_last_vchain_head(VersionChainHead *vchain_head) {
  if (vchain_head == &entries_[ENTRY_CAPACITY - 1])
    return true;
  else
    return false;
}

VersionChainHead *VersionChainHeadBlock::get_vchain_head(TableScanCursor *scan_cursor) {
  assert(scan_cursor->block_id_ == block_id_);
  return &entries_[scan_cursor->idx_in_block_];
}

}  // namespace db20xx
