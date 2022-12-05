#pragma once
#include "version_chain.h"
#include "record.h"

namespace fulgurdb {
char *VersionChainHead::get_latest_record_payload() {
  return latest_record_->get_payload();
}

void VersionChainHead::init() { latest_record_ = nullptr; }

int VersionChainHeadBlock::alloc_vchain_head(VersionChainHead *&vchain_head) {
  uint32_t offset = valid_entry_num_.fetch_add(1, std::memory_order_relaxed);
  if (offset >= RECORD_PTR_CAPACITY) return FULGUR_BLOCK_FULL;
  vchain_head = &entries_[offset];

  return FULGUR_SUCCESS;
}

bool VersionChainHeadBlock::is_last_vchain_head(VersionChainHead *vchain_head) {
  if (vchain_head == &entries_[RECORD_PTR_CAPACITY - 1])
    return true;
  else
    return false;
}
}  // namespace fulgurdb
