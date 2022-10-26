#pragma once
#include "./utils.h"
#include "./field.h"

namespace fulgurdb {
class Schema {
friend class Table;
public:
  Schema() {}
  Schema(const Schema &other):fields_(other.fields_){}

  void add_field(Field &field) {
    fields_.push_back(field);
    total_size_ += field.data_bytes_;
  }

  uint32_t field_num() const {
    return fields_.size();
  }

  const Field& get_field(uint32_t field_idx) const {
    // assert field_idx < field_num()
    return fields_[field_idx];
  }

  void set_null_byte_length(uint32_t len) {
    null_byte_length_ = len;
  }

  uint32_t get_null_byte_length() const {
    return null_byte_length_;
  }

private:
  std::vector<Field> fields_;
  uint32_t total_size_;
  uint32_t null_byte_length_;
};
}
