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
Table* Database::create_table(const std::string &table_name, Schema &schema) {
  if (check_table_existence(table_name) == true) {
    return nullptr;
  }
  Table *table = new Table(table_name, schema);
  tables_[table_name] = table;

  return table;
}

Table *Database::get_table(const std::string table_name) {
  if (check_table_existence(table_name) == false)
    return nullptr;
  else
    return tables_[table_name];
}

}

