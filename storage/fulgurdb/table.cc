#include "./table.h"

namespace fulgurdb {

std::unique_ptr<Tuple> Table::pre_allocate_tuple() {
  char *data = (char *)malloc(schema_.total_size_);
  rows_.push_back(data);
  return std::unique_ptr<Tuple>(new Tuple(data));
}

}
