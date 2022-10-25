#pragma once
#include "utils.h"
#include "schema.h"

namespace fulgurdb {

class Tuple {
public:
  Tuple(char *data):data_(data) {}
  void load_data_from_mysql(char *packed_data, const Schema &schema);
private:
  char *data_;
};

}
