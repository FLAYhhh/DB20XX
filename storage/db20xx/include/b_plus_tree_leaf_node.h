#pragma once

#include <string>
#include <utility>
#include <vector>
#include "index.h"
#include "b_plus_tree_node.h"

namespace db20xx {

using LeafNodeMappingType = std::pair<Key, VersionChainHead *>;
#define LEAF_NODE_HEADER_SIZE 16
#define LEAF_NODE_SIZE ((DB20XX_NODE_SIZE - LEAF_NODE_HEADER_SIZE) / sizeof(LeafNodeMappingType))

/**
 * Store indexed key and VersionChainHead(see MVCC of In-memory database)
 * together within leaf node. Only support unique key.
 *
 * Leaf node format (keys are stored in order):
 *  ----------------------------------------------------------------------
 * | HEADER | KEY(1) + VersionChainHead(1) | KEY(2) + VersionChainHead(2) | ... | KEY(n) + VersionChainHead(n)
 *  ----------------------------------------------------------------------
 *
 *  Header format (size in byte):
 *  ---------------------------------------------------------------------
 * | NodeType (4) | rwlock | CurrentSize (4) | MaxSize (4) |
 *  ---------------------------------------------------------------------
 *  -----------------------------------------------
 * |  NextNodeId (8)
 *  -----------------------------------------------
 */
class BPlusTreeLeafNode : public BPlusTreeNode {
 public:
  // Delete all constructor / destructor to ensure memory safety
  BPlusTreeLeafNode() = delete;
  BPlusTreeLeafNode(const BPlusTreeLeafNode &other) = delete;

  /**
   * After creating a new leaf node from buffer pool, must call initialize
   * method to set default values
   * @param max_size Max size of the leaf node
   */
  void Init(int max_size = LEAF_NODE_SIZE);

  // helper methods
  auto GetNextNode() const -> BPlusTreeLeafNode*;
  void SetNextNodeId(BPlusTreeLeafNode *next_node_id);
  auto KeyAt(int index) const -> KeyType;

  /**
   * @brief for test only return a string representing all keys in
   * this leaf node formatted as "(key1,key2,key3,...)"
   *
   * @return std::string
   */
  auto ToString() const -> std::string {
    std::string kstr = "(";
    bool first = true;

    for (int i = 0; i < GetSize(); i++) {
      KeyType key = KeyAt(i);
      if (first) {
        first = false;
      } else {
        kstr.append(",");
      }

      kstr.append(key.s, key.len);
    }
    kstr.append(")");

    return kstr;
  }

 private:
  BPlusTreeLeafNode *next_node_;
  // Flexible array member for node data.
  LeafNodeMappingType array_[0];
};
}  // namespace db20xx

