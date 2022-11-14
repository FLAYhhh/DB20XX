#pragma once
#include "utils.h"
#include "schema.h"

namespace fulgurdb {

class RecordLocation {
friend class Table;
public:
  RecordLocation() {}
  RecordLocation(char *data):data_(data) {}
  void load_data_from_mysql(char *mysql_row_data, const Schema &schema);
  void load_data_to_mysql(char *mysql_row_data, const Schema &schema);

  const char *get_record() const {
    return data_;
  }
private:
  char *data_ = nullptr;
};

}
