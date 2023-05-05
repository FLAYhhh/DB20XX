#include "index.h"
#include <deque>

namespace db20xx {

class BplusTreeIndex : public Index {
  friend class Table;

 public:
  BplusTreeIndex(void) {}
  BplusTreeIndex(const KeyInfo &keyinfo) : Index(keyinfo) {}
  ~BplusTreeIndex() {}

  /**
  @brief
    put a key-value pair to B+ tree
  */
  bool put([[maybe_unused]] const Key &key, [[maybe_unused]] VersionChainHead *vchain_head) override {
    //TODO
    return false;
  }

  /**
    @brief
      given a key, get the value
    @return values
      @retval1 true: key exists
      @retval2 false: key doesnot exist
  */
  bool get([[maybe_unused]] const Key &key, [[maybe_unused]] VersionChainHead *&vchain_head) const override {
    //TODO:
    return false;
  }

  /** @remove a <key,value> pair from b+ tree */
  bool remove([[maybe_unused]] const Key &key) override {
    //TODO
    return false;
  }

  /** @Given a key, locate to the first tuple greater or equal to key, depending on emit_firstkey.
   * And save the scan state into ScanIterator */
  bool scan_range_first([[maybe_unused]] const Key &key, [[maybe_unused]] VersionChainHead *&vchain_head,
                        [[maybe_unused]] bool emit_firstkey, [[maybe_unused]] ScanIterator &iter) const override {
    //TODO
    return false;
  }

  /** @Use iterator to get the next value. If no next value, return false. */
  bool scan_range_next([[maybe_unused]] VersionChainHead *&vchain_head, [[maybe_unused]] ScanIterator &iter) const override {
    //TODO
    return false;
  }

  /** @reverse direction */
  bool rscan_range_first([[maybe_unused]] const Key &key, [[maybe_unused]] VersionChainHead *&vchain_head,
                         [[maybe_unused]] bool emit_firstkey, [[maybe_unused]] ScanIterator &iter) const override {
    //TODO
    return false;
  }

  /** @reverse direction */
  bool rscan_range_next([[maybe_unused]] VersionChainHead *&vchain_head, [[maybe_unused]] ScanIterator &iter) const override {
    //TODO
    return false;
  }

 private:
  //TODO:
};
}
