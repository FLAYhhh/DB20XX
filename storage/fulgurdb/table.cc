#include "./table.h"

namespace fulgurdb {

std::unique_ptr<Tuple> Table::pre_allocate_tuple() {
  char *data = (char *)malloc(schema_.total_size_);
  rows_.push_back(data);
  return std::unique_ptr<Tuple>(new Tuple(data));
}

char* Table::get_row_data(uint32_t row_idx) const {
  if (row_idx >= rows_.size())
    return nullptr;
  else
    return rows_[row_idx];
}

}
