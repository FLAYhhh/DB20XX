#pragma once
#include "./utils.h"
#include "./database.h"

namespace fulgurdb {

/**
  @brief Engine是对Fulgurdb存储引擎的全局抽象。其功能与handlerton类型的
         fulgurdb_hton类似,不过handlerton主要服务于sl(server layer)和se(st
         -orage engine)之间的接口层，而Engine的定义和实现应当是独立的。
         handlerton中方法的实现可以借助于Engine中的方法并进行一定程度的
         包装。
*/
class Engine {
public:
  /** @brief 获取fulgurdb存储引擎的单例
  */
  // static Engine& GetInstance();

  /** @brief 初始化fulgurdb存储引擎
  */
  static void init() {}

/*===============methods for database==================*/
  static bool check_database_existence(const std::string &db_name);
  static Database* create_new_database(const std::string &db_name);

  static Database* get_database(const std::string &db_name);

private:
  static std::mutex databases_lock_;
  static std::unordered_map<std::string, Database*> databases_;
};

}
