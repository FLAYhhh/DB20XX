#pragma once
#include <cstdint>
#include "record.h"
#include "transaction.h"
#include "utils.h"
#include "version_chain.h"
#include "thread_context.h"
#include "masstree-beta/str.hh"

namespace db20xx {

using namespace Masstree::lcdf;
typedef Str Key;

struct KeyInfo {
  /**
    mysql keypart counted from 1,
    db20xx keypart counted from 0;
  */
  void add_key_part(uint32_t key_part) { key_parts.push_back(key_part - 1); }
  uint32_t get_key_length() { return key_len; }

  Schema schema;
  std::vector<int> key_parts;
  uint32_t key_len = 0; //key length capacity
};

//TODO
class ScanIterator {
public:
  const Key &get_current_key();
  VersionChainHead *get_current_value();
  void reset();
};

class Index {
  friend class Table;

 public:
  Index(void) {}
  Index(const KeyInfo &keyinfo) : keyinfo_(keyinfo) {}
  ~Index() {}

  virtual bool get(const Key &key, VersionChainHead *&vchain_head) const = 0;

  virtual bool put(const Key &key, VersionChainHead *vchain_head) = 0;

  virtual bool remove(const Key &key) = 0;

  virtual bool scan_range_first(const Key &key, VersionChainHead *&vchain_head,
                        bool emit_firstkey, ScanIterator &iter) const = 0;

  virtual bool scan_range_next(VersionChainHead *&vchain_head, ScanIterator &iter) const = 0;

  virtual bool rscan_range_first(const Key &key, VersionChainHead *&vchain_head,
                         bool emit_firstkey, ScanIterator &iter) const = 0;

  virtual bool rscan_range_next(VersionChainHead *&vchain_head, ScanIterator &iter) const = 0;

  /**
  @brief
    build key from a db20xx record
    have memory allocation in this function, need to free by others
  @args
    arg1 record: record payload, without record header
  */
  void build_key(const char *record, Key &output_key, ThreadContext *thd_ctx) {
    char *key_data = thd_ctx->get_key_container();
    char *key_cursor = key_data;
    uint32_t key_len = 0;
    for (auto i : keyinfo_.key_parts) {
      const Field &field = keyinfo_.schema.get_field(i);
      const char *field_data = nullptr;
      uint32_t data_len = 0;

      field.get_field_data(record, field_data, data_len);
      memcpy(key_cursor, field_data, data_len);
      key_cursor += data_len;
      key_len += data_len;
    }

    output_key.s = key_data;
    output_key.len = key_len;
  }

  void build_key_from_mysql_record(const char *mysql_record, Key &output_key, ThreadContext *thd_ctx) {
    char *key_data = thd_ctx->get_key_container();
    char *key_cursor = key_data;
    uint32_t key_len = 0;
    for (auto i: keyinfo_.key_parts) {
      const Field &field = keyinfo_.schema.get_field(i);
      const char *field_data = nullptr;
      uint32_t data_len = 0;

      field.get_mysql_field_data(mysql_record, field_data, data_len);
      memcpy(key_cursor, field_data, data_len);
      key_cursor += data_len;
      key_len += data_len;
    }
    output_key.s = key_data;
    output_key.len = key_len;
  }

  uint32_t get_key_length() { return keyinfo_.get_key_length(); }
  const KeyInfo &get_key_info() const { return keyinfo_; }

 protected:
  KeyInfo keyinfo_;
};


}  // namespace db20xx
