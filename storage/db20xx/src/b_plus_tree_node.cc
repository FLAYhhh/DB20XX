#include "b_plus_tree_node.h"

namespace db20xx {

/*
 * Helper methods to get/set node type
 * Node type enum class is defined in b_plus_tree_node.h
 */
auto BPlusTreeNode::IsLeafNode() const -> bool { return false; }
void BPlusTreeNode::SetNodeType(IndexNodeType node_type) {}

/*
 * Helper methods to get/set size (number of key/value pairs stored in that
 * node)
 */
auto BPlusTreeNode::GetSize() const -> int { return 0; }
void BPlusTreeNode::SetSize(int size) {}
void BPlusTreeNode::IncreaseSize(int amount) {}

/*
 * Helper methods to get/set max size (capacity) of the node
 */
auto BPlusTreeNode::GetMaxSize() const -> int { return 0; }
void BPlusTreeNode::SetMaxSize(int size) {}

/*
 * Helper method to get min node size
 * Generally, min node size == max node size / 2
 */
auto BPlusTreeNode::GetMinSize() const -> int { return 0; }

}  // namespace db20xx

