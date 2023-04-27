#include "../include/engine.h"

namespace db20xx {

#if 0
Engine& Engine::GetInstance() {
  static Engine the_engine;
  return the_engine;
}
#endif

std::mutex Engine::databases_lock_;
std::unordered_map<std::string, Database*> Engine::databases_;

bool Engine::check_database_existence(const std::string &db_name) {
  if (databases_.find(db_name) != databases_.end())
    return true;
  else
    return false;
}


Database* Engine::create_new_database(const std::string &db_name) {
  // TODO: concurrency control
  Database *db = new Database(db_name);
  databases_[db_name] = db;
  return db;
}

Database* Engine::get_database(const std::string &db_name) {
  if (check_database_existence(db_name) == false)
    return nullptr;
  else
    return databases_[db_name];
}

}
