# FulgurDB

## 介绍
为社区版MySQL 8.0.27研发一个新的内存存储引擎FulgurDB，能够替代MySQL原有的InnoDB和Memory存储引擎，充分发挥大内存服务器的性能。

-FulgurDB引擎需支持事务处理功能；
-所提交项目文档及源代码需符合GPL V2开源协议；
-如在项目中使用第三方开源代码，需确认开源协议是否有冲突；

|**功能**	    |**InnoDB**	|**Memory**	|**FulgurDB**   |
|:-------------:|:-----------:|:-----------:|:-------------:|
|索引类型		|B-Tree索引	|B-Tree索引	|Mass-Tree索引|
|数据压缩		|支持		|不支持		|不支持       |
|数据缓存		|支持		|N/A		|N/A          |
|索引缓存		|支持		|N/A		|N/A          |
|外键			|支持		|不支持		|不支持       |
|全文检索		|支持		|不支持		|不支持       |
|空间数据类型	|支持		|不支持		|不支持       |
|空间数据索引	|支持		|不支持		|不支持       |
|锁粒度			|行级锁		|表锁		|行级锁       |
|MVCC			|支持		|不支持		|支持         |
|数据存储限制	|64TB		|内存限制	|内存限制     |
|事务处理		|支持		|不支持		|支持         |
|日志			|WAL		|不支持		|WAL          |
|数据恢复		|基于日志	|不支持		|基于日志     |
|检查点			|支持		|不支持		|支持         |
|垃圾版本回收	|支持		|N/A		|支持         |


## 系统架构
（待完善）



## 使用说明
### 1. 编译
**Tool chain version**
- g++-10
- cmake 3.24.0

```shell
$ mkdir build
$ cd build
$ cmake .. -DWITH_BOOST=../boost -DWITH_DEBUG=1
$ make -j12
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
datadir=/home/<USER>/.fulgurdb/data
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

## TODO List
### Masstree索引
1. Masstree叶子节点的value type实现过于简陋.具体体现在两个方面:
  - 没有考虑并发场景
  - 内存管理: 包含内存分配和垃圾回收
  后续需要参照masstree原生支持的value type完善现有实现

2. 当前还未适配索引的remove操作


### Storage Layout

### 并发控制

### 日志和检查点
