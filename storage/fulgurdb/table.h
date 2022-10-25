#pragma once
#include "utils.h"
#include "./schema.h"
#include "./tuple.h"

namespace fulgurdb {
class Table {
public:
  Table(const std::string &table_name, Schema &schema):
     table_name_(table_name), schema_(schema){}

  const Schema& get_schema() const {return schema_;}
  std::unique_ptr<Tuple> pre_allocate_tuple();


  char* get_row_data(uint32_t row_idx) const;

private:
  std::string table_name_;
  Schema schema_;
  std::vector<char *> rows_;
};
}
