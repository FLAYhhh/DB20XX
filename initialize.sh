rm -rf $(HOME)/.fulgurdb/data
mkdir -p $(HOME)/.fulgurdb/data
build/bin/mysqld --defaults-file=my.cnf --initialize-insecure
