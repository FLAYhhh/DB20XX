#include "../include/epoch.h"

namespace db20xx {

std::atomic<uint64_t> GlocalEpochManager::current_global_epoch_id_{1};
std::atomic<uint32_t> GlocalEpochManager::next_global_txn_id_{1};
std::unordered_map<int, LocalEpochManager*> GlocalEpochManager::local_epochs_;

}
