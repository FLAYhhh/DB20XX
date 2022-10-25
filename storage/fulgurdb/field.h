#pragma once
#include "./utils.h"
#include "./data_types.h"

namespace fulgurdb {

/**
*/
class Field {
friend class Schema;
public:
  static const bool STORE_INLINE = true;
  static const bool STORE_NON_INLINE = false;
  Field(TYPE_ID type_id, std::string name, uint32_t data_bytes, bool store_inline):
    field_type_id_(type_id),
    field_name_(name),
    data_bytes_(data_bytes),
    store_inline_(store_inline) {}

  bool store_inline() const {
    return store_inline_;
  }

  uint32_t get_data_bytes() const {
    return data_bytes_;
  }

  void set_mysql_length_bytes(uint32_t length_bytes) {
    mysql_length_bytes_ = length_bytes;
  }

  uint32_t get_mysql_length_bytes() const {
    return mysql_length_bytes_;
  }

  TYPE_ID get_field_type() const {
    return field_type_id_;
  }

private:
  TYPE_ID field_type_id_;
  std::string field_name_;
  uint32_t data_bytes_;
  bool store_inline_;

  // Field中需要额外存储一些MySQL server层中Field的元数据.
  // 当MySQL的row buf存储到fulgurdb中时, 如果该field为变长数据类型，
  // 我们需要知道该field用多少个字节记录数据长度。
  uint32_t mysql_length_bytes_;
};
}
