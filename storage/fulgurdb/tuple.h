#pragma once
#include "utils.h"
#include "schema.h"

namespace fulgurdb {

class Tuple {
public:
  Tuple(char *data):data_(data) {}
  void load_data_from_mysql(char *mysql_row_data, const Schema &schema);
  void load_data_to_mysql(char *mysql_row_data, const Schema &schema);
private:
  char *data_;
};

}
