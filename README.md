# kv on NVM

**本程序需要读取位于/mnt/mem下的NVM设备**

## 构建与测试
**请在项目的根目录下运行以下命令**

### 编译ycsb和gtest测试
```bash
make all
```
### 只编译ycsb测试
```bash
make ycsb
```
### 只编译gtest测试
```bash
make ehash
```

### 运行ycsb测试
```bash
sudo ./bin/ycsb
```

### 运行gtest测试
```bash
sudo ./bin/ehash_test
```