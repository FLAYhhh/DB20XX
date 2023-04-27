#pragma once
#include "./utils.h"
#include "./field.h"

namespace db20xx {
class Schema {
friend class Table;
public:
  //Schema() = default;
  //Schema(const Schema &other) = default;


  /**
  @brief 顺序地向schema中加入一个field
  */
  void add_field(Field &field) {
    fields_.push_back(field);
    total_size_ += field.data_bytes_;
  }

  /**
  @brief 返回schema中field的数量
  */
  uint32_t field_num() const {
    return fields_.size();
  }

  /**
  @brief 给定field在schema中的序号,获取该field
  */
  const Field& get_field(uint32_t field_idx) const {
    // assert field_idx < field_num()
    return fields_[field_idx];
  }

  /**
  @brief 设置schema中,null bit map占用的字节数
         null的实现方式和mysql相同
  */
  void set_null_byte_length(uint32_t len) {
    null_byte_length_ = len;
    total_size_ += len;
  }

  /**
  @brief 获取schema中 null bit map占用的字节数
  */
  uint32_t get_null_byte_length() const {
    return null_byte_length_;
  }

  uint32_t get_column_offset(uint32_t idx) {
    return fields_[idx].off_in_record_;
  }

  uint32_t get_column_length(uint32_t idx) {
    return fields_[idx].data_bytes_;
  }

  uint32_t get_record_data_length() {
    return total_size_;
  }

private:
  std::vector<Field> fields_;

  // total_size_ = null_byte_length + all fields bytes
  uint32_t total_size_ = 0;
  uint32_t null_byte_length_ = 0;
};
}
