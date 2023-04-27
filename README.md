# DB20XX

## 介绍
## 系统架构
## 使用说明
### 1. 编译

```shell
$ mkdir build
$ cd build
$ cmake .. -DWITH_BOOST=../boost -DWITH_DEBUG=1
$ make
```

### 2. 修改mysql配置文件

```shell
## 从build目录回到项目根目录
$ cd ..
## 修改mysql配置文件
$ vim my.cnf
---------------------------------
[mysqld]
port=3306
datadir=/home/<USER>/.db20xx/data
---------------------------------
将<USER>替换为当前登陆的用户名
```

### 3. 初始化mysql

```shell
$ sh initialize.sh
```

### 4. Debug mysql

使用GDB运行mysqld(mysql server):

```shell
$ sh debug_mysqld.sh
## 进入gdb界面
(gdb) ...
```

另起一个窗口运行mysql client:

```shell
$ build/bin/mysql -uroot
## 进入mysql client界面
> ...
```
