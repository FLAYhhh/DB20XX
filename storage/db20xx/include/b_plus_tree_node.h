#pragma once
namespace db20xx {

#define DB20XX_NODE_SIZE 4096
enum class IndexNodeType { INVALID_INDEX_NODE = 0, LEAF_NODE, INTERNAL_NODE };

/**
 * Both internal and leaf node are inherited from this node.
 *
 * It actually serves as a header part for each B+ tree node and
 * contains information shared by both leaf node and internal node.
 *
 * Header format (size in byte, 12 bytes in total):
 * ----------------------------------------------------------------------------
 * | nodeType (4) | CurrentSize (4) | MaxSize (4) |
 * ----------------------------------------------------------------------------
 */
class BPlusTreeNode {
 public:
  // Delete all constructor / destructor to ensure memory safety
  BPlusTreeNode() = delete;
  BPlusTreeNode(const BPlusTreeNode &other) = delete;
  ~BPlusTreeNode() = delete;

  auto IsLeafNode() const -> bool;
  void SetNodeType(IndexNodeType node_type);

  auto GetSize() const -> int;
  void SetSize(int size);
  void IncreaseSize(int amount);

  auto GetMaxSize() const -> int;
  void SetMaxSize(int max_size);
  auto GetMinSize() const -> int;

 private:
  // member variable, attributes that both internal and leaf node share
  IndexNodeType node_type_ __attribute__((__unused__));
  int size_ __attribute__((__unused__));
  int max_size_ __attribute__((__unused__));
};

}
