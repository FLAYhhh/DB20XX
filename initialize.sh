rm -rf ${HOME}/usr/.fulgurdb/data
mkdir -p ${HOME}/usr/.fulgurdb/data
build/bin/mysqld --defaults-file=my.cnf --initialize-insecure
