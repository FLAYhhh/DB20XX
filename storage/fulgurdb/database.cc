#include "./database.h"

namespace fulgurdb {

bool Database::check_table_existence(const std::string &table_name) {
  if (tables_.find(table_name) != tables_.end())
    return true;
  else
    return false;
}


/**
@return
  retval 0 success
*/
int Database::create_table(const std::string &table_name, Schema &schema) {
  if (check_table_existence(table_name) == true) {
    return -1;
  }
  Table *table = new Table(table_name, schema);
  tables_[table_name] = table;

  return 0;
}

}
