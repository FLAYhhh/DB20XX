# DB20XX
## 使用说明
### 1. 编译

```shell
$ mkdir build
$ cd build
$ cmake .. -DWITH_BOOST=../boost -DWITH_DEBUG=1
$ make -j `nproc`
```
### 2. 生成mysqld配置文件&初始化mysqld

```shell
$ sh initialize.sh
```

### 3. 运行mysqld

```shell
$ sh run_mysqld.sh
```
