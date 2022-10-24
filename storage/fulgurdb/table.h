#pragma once
#include "utils.h"
#include "./schema.h"

namespace fulgurdb {
class Table {
public:
  Table(const std::string &table_name, Schema &schema):
     table_name_(table_name), schema_(schema){}

private:
  std::string table_name_;
  Schema schema_;
};
}
