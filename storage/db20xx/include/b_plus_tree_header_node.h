#pragma once
#include "b_plus_tree_node.h"
#include "utils.h"

namespace db20xx {

class BPlusTreeHeaderNode {
 public:
  // Delete all constructor / destructor to ensure memory safety
  BPlusTreeHeaderNode() = delete;
  BPlusTreeHeaderNode(const BPlusTreeHeaderNode &other) = delete;

  BPlusTreeNode* root_node_;
  ReaderWriterLatch rwlock_;
};

}  // namespace db20xx

