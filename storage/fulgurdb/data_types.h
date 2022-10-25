#pragma once
#include "./utils.h"

namespace fulgurdb {
const int NONE_INLINE_PTR_SIZE = 8;
enum TYPE_ID {
  TINYINT_ID = 0,
  SMALLINT_ID,
  MEDIUMINT_ID,
  INT_ID,
  BIGINT_ID,
  DECIMAL_ID,
  FLOAT_ID,
  DOUBLE_ID,
  VARCHAR_ID,
  MEDIUMTEXT_ID,
  YEAR_ID,
  DATE_ID,
  TIME_ID,
  DATETIME_ID,
  TIMESTAMP_ID,
  TYPE_ID_UPBOUND
};
const int FURGUR_TYPE_NUMS = TYPE_ID_UPBOUND;

#if 0
class BASE_TYPE {
public:
  static BASE_TYPE *get_type(enum TypeID) {
    return type_singletons_[TypeID];
  }

  virtual uint32_t occupied_bytes() = 0;
  virtual bool store_inline() = 0;

private:
  static BASE_TYPE *type_singletons_[FURGUR_TYPE_NUMS] = {
    TINYINT::get_singleton(),
    SMALLINT()::get_singleton(),
    MEDIUMINT()::get_singleton(),
    INT::get_singleton(),
    BIGINT::get_singleton(),
    DECIMAL::get_singleton(),
    FLOAT::get_singleton(),
    DOUBLE::get_singleton(),
    VARCHAR::get_singleton(),
    MEDIUMTEXT::get_singleton(),
    YEAR::get_singleton(),
    DATE::get_singleton(),
    TIME::get_singleton(),
    DATETIME::get_singleton(),
    TIMESTAMP::get_singleton()
  }
};

class TINYINT: public BASE_TYPE {
public:
  static TINYINT *get_singleton() {
    static TINYINT the_one;
    return &the_one;
  }

  uint32_t occupied_bytes() override {
    return 1;
  }

  bool store_inline() override {
    return true;
  }
};

class SMALLINT {
public:
  static SMALLINT *get_singleton() {
    static SMALLINT the_one;
    return &the_one;
  }

  uint32_t occupied_bytes() override {
    return 2;
  }

  bool store_inline() override {
    return true;
  }
};

// can only use 3 bytes
// max_value_ = 1 << 24 - 1;
class MEDIUMINT {
public:
  static MEDIUMINT *get_singleton() {
    static MEDIUMINT the_one;
    return &the_one;
  }

  uint32_t occupied_bytes() override {
    return 3;
  }

  bool store_inline() override {
    return true;
  }
};

class INT {
public:
  static INT *get_singleton() {
    static INT the_one;
    return &the_one;
  }

  uint32_t occupied_bytes() override {
    return 4;
  }

  bool store_inline() override {
    return true;
  }
};

class BIGINT {
public:
  static BIGINT *get_singleton() {
    static BIGINT the_one;
    return &the_one;
  }

  uint32_t occupied_bytes() override {
    return 8;
  }

  bool store_inline() override {
    return true;
  }
};

//FIXME: 阅读mysql文档，确定剩余类型的存储方式
#define OCCUPY_BYTES_UNKOWN 0
#define STORE_INLINE_UNKOWN true

class DECIMAL {
public:
  static DECIMAL *get_singleton() {
    static DECIMAL the_one;
    return &the_one;
  }

  uint32_t occupied_bytes() override {
    return 8;
  }

  bool store_inline() override {
    return false;
  }

};

class FLOAT {
public:
  static FLOAT *get_singleton() {
    static FLOAT the_one;
    return &the_one;
  }

  // FIXME:FLOAT(p) 4 bytes if 0 <= p <= 24, 8 bytes if 25 <= p <= 53
  uint32_t occupied_bytes() override {
    return 8;
  }

  bool store_inline() override {
    return true;
  }

};

class DOUBLE {
public:
  static DOUBLE *get_singleton() {
    static DOUBLE the_one;
    return &the_one;
  }

  uint32_t occupied_bytes() override {
    return 8;
  }

  bool store_inline() override {
    return true;
  }

};

class VARCHAR {
public:
  static VARCHAR *get_singleton() {
    static VARCHAR the_one;
    return &the_one;
  }

  uint32_t occupied_bytes() override {
    return 8;
  }

  bool store_inline() override {
    return false;
  }
};

class MEDIUMTEXT {
public:
  static MEDIUMTEXT *get_singleton() {
    static MEDIUMTEXT the_one;
    return &the_one;
  }

  uint32_t occupied_bytes() override {
    return 8;
  }

  bool store_inline() override {
    return false;
  }
};

class YEAR {
public:
  static YEAR *get_singleton() {
    static YEAR the_one;
    return &the_one;
  }

  uint32_t occupied_bytes() override {
    return 1;
  }

  bool store_inline() override {
    return true;
  }
};

class DATE {
public:
  static DATE *get_singleton() {
    static DATE the_one;
    return &the_one;
  }

  uint32_t occupied_bytes() override {
    return 3;
  }

  bool store_inline() override {
    return true;
  }
};

class TIME {
public:
  static TIME *get_singleton() {
    static TIME the_one;
    return &the_one;
  }

  uint32_t occupied_bytes() override {
    return 8;
  }

  bool store_inline() override {
    return true;
  }
};

class DATETIME {
public:
  static DATETIME *get_singleton() {
    static DATETIME the_one;
    return &the_one;
  }

  uint32_t occupied_bytes() override {
    return 8;
  }

  bool store_inline() override {
    return true;
  }
};

class TIMESTAMP {
public:
  static TIMESTAMP *get_singleton() {
    static TIMESTAMP the_one;
    return &the_one;
  }

  uint32_t occupied_bytes() override {
    return 8;
  }

  bool store_inline() override {
    return true;
  }
};

#endif

}
