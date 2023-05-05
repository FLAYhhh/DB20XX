#include "index.h"

namespace db20xx {
class BplusTreeIndex : public Index {
  friend class Table;

 public:
  BplusTreeIndex(void) {}
  BplusTreeIndex(const KeyInfo &keyinfo) : Index(keyinfo) {}
  ~BplusTreeIndex() {}

  /**
  @brief
    put a key-value pair to masstree. key consists of columns, values is
    corresponding RecordLocation of that Record.

  @return values
    @retval1 true: first put
    @retval2 false: not first put, update old value
  */
  bool put([[maybe_unused]] const Key &key, [[maybe_unused]] VersionChainHead *vchain_head) override {
    //TODO
    return false;
  }

  /**
    @brief
      given key, get the value(RecordLocation of a db20xx row) of the key
    @return values
      @retval1 true: key exists
      @retval2 false: key doesnot exist
  */
  bool get([[maybe_unused]] const Key &key, [[maybe_unused]] VersionChainHead *&vchain_head) const override {
    //TODO:
    return false;
  }

  bool remove([[maybe_unused]] const Key &key) override {
    //TODO
    return false;
  }

  bool scan_range_first([[maybe_unused]] const Key &key, [[maybe_unused]] VersionChainHead *&vchain_head,
                        [[maybe_unused]] bool emit_firstkey, [[maybe_unused]] ScanIterator &iter) const override {
    //TODO
    return false;
  }

  bool scan_range_next([[maybe_unused]] VersionChainHead *&vchain_head, [[maybe_unused]] ScanIterator &iter) const override {
    //TODO
    return false;
  }

  bool rscan_range_first([[maybe_unused]] const Key &key, [[maybe_unused]] VersionChainHead *&vchain_head,
                         [[maybe_unused]] bool emit_firstkey, [[maybe_unused]] ScanIterator &iter) const override {
    //TODO
    return false;
  }

  bool rscan_range_next([[maybe_unused]] VersionChainHead *&vchain_head, [[maybe_unused]] ScanIterator &iter) const override {
    //TODO
    return false;
  }

 private:
  //TODO:
};
}
