#pragma once
#include "./utils.h"
#include "./table.h"
namespace fulgurdb {
class Database{
public:
  Database(std::string db_name):db_name_(db_name){}
  bool check_table_existence(const std::string &table_name);

/**
@return retval-0: success
*/
  int create_table(const std::string &table_name, Schema &schema);
private:
  std::string db_name_;
  std::mutex tables_lock_;
  std::unordered_map<std::string, Table*> tables_;
/*
 public:
  Database() = delete;
  Database(Database const &) = delete;
  Database(const uint database_oid);
  ~Database();
  uint GetOid() const { return database_oid; }
  void AddTable(DataTable *table, bool is_catalog = false);
  DataTable *GetTable(const uint table_offset) const;
  DataTable *GetTableWithOid(const uint table_oid) const;
  DataTable *GetTableWithName(const std::string table_name) const;
  uint GetTableCount() const;
  void DropTableWithOid(const uint table_oid);
  const std::string GetInfo() const;
  std::string GetDBName();
  void setDBName(const std::string &database_name);
 private:
  const uint database_oid;
  std::string database_name;
  std::vector<DataTable *> tables;
  std::mutex database_mutex;
*/
};
}
