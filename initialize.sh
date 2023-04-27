rm -rf ${HOME}/usr/.db20xx/data
mkdir -p ${HOME}/usr/.db20xx/data
build/bin/mysqld --defaults-file=my.cnf --initialize-insecure
