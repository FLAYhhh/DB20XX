PROJECT (ICU-stubdata)

SET_DIRECTORY_PROPERTIES(PROPERTIES INCLUDE_DIRECTORIES "${PROJECT_SOURCE_DIR}/../common")

ADD_LIBRARY(icustubdata STATIC stubdata.cpp)

IF(LINUX OR (SOLARIS AND MY_COMPILER_IS_GNU_OR_CLANG))
  TARGET_COMPILE_OPTIONS(icustubdata PRIVATE ${ICU_LINUX_COMPILE_OPTIONS})
ENDIF()
