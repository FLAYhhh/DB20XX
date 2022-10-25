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

private:
  std::vector<Field> fields_;
  uint32_t total_size_;
};
}
