#pragma once
#include "epoch_transaction_id.h"

namespace fulgurdb {

std::atomic<uint64_t> GlocalEpochManager::current_global_epoch_id_ = 0;
std::atomic<uint32_t> GlocalEpochManager::next_global_txn_id_ = 0;

}
