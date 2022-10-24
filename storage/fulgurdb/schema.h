#pragma once
#include "./utils.h"
#include "./field.h"

namespace fulgurdb {
class Schema {
public:
  Schema() {}
  Schema(const Schema &other):fields_(other.fields_){}

  void add_field(Field &field) {
    fields_.push_back(field);
  }

private:
  std::vector<Field> fields_;
};
}
