#pragma once
#include "./utils.h"
#include "./data_types.h"

namespace fulgurdb {
/**
*/
class Field {
public:
  Field(std::string name, uint32_t length, bool in_place):
    field_name_(name), field_length_(length), in_place_(in_place) {}

private:
//  TypeID filed_type_id_;
  std::string field_name_;
  uint32_t field_length_;
  bool in_place_;

// if in_place == false, store value in data;
//  char *data_;
//  uint32_t data_length_;
};
}
