# Bitcoin Core 开发项目上下文

## 项目概述

Bitcoin Core 是比特币网络的参考实现，它是一个去中心化的点对点网络客户端，用于下载和完全验证区块和交易。该项目包含一个可选的钱包和图形用户界面。

这是一个使用 MIT 许可证的开源项目，代码库主要用 C++ 编写，具有多线程架构，支持命令行界面 (CLI)、守护进程 (daemon)、图形界面 (GUI) 和 RPC 接口。

## 主要技术栈

- **编程语言**: C++20 (核心部分), Python (测试脚本)
- **构建系统**: CMake (版本 3.22+)
- **依赖管理**: 通过 `depends` 目录管理外部依赖
- **测试框架**: CTest (单元测试), Python (集成/功能测试), 模糊测试 (fuzzing)
- **代码格式**: clang-format, clang-tidy
- **GUI**: Qt 6.2+

## 项目架构

Bitcoin Core 是一个单体代码库，包含多个组件：

- **bitcoind**: 核心守护进程，连接比特币网络，验证和中继交易和区块
- **bitcoin-cli**: 命令行客户端，与运行中的 bitcoind 进程通信
- **bitcoin-qt**: 图形界面客户端
- **bitcoin-tx**: 交易处理工具
- **bitcoin-util**: 实用工具集合
- **钱包功能**: 可选的钱包组件

## 主要目录结构

- `src/`: 核心源代码
- `test/`: 单元测试和功能测试
- `doc/`: 文档
- `contrib/`: 贡献脚本和工具
- `depends/`: 依赖管理
- `share/`: 共享资源

## 开发约定

### C++ 编码风格

- 使用 clang-format 格式化代码 (规则在 `src/.clang-format`)
- 使用 4 空格缩进 (非命名空间)
- 大括号规则：
  - 类、函数、方法使用新行大括号
  - 其他使用同一行大括号
- 变量命名: snake_case (成员变量使用 `m_` 前缀，全局变量使用 `g_` 前缀)
- 常量命名: ALL_CAPS
- 类和函数命名: UpperCamelCase
- 首选 `nullptr` 而不是 `NULL` 或 `(void*)0`
- 首选 `++i` 而不是 `i++`
- 使用 `static_assert` 进行编译时检查

### 提交约定

- 提交信息应包含简短的主题行 (最多 50 字符)，空行，然后是详细的解释
- 提交应该是原子的，易于阅读的差异
- 每个提交都应该能够独立构建，没有警告或错误
- 前缀应该反映影响的组件 (如 `consensus:`, `net:`, `wallet:`, `qt:`, `rpc:` 等)

### 测试

- 强烈鼓励为新代码编写单元测试
- 运行单元测试: `ctest`
- 运行功能测试: `test/functional/test_runner.py`
- 运行模糊测试: `test/fuzz/test_runner.py`

## 构建和运行

### 构建项目

```bash
# 配置项目
cmake -B build
# 或指定构建类型
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo

# 构建
cmake --build build
```

### 重要构建选项

- `BUILD_DAEMON=ON`: 构建 bitcoind 可执行文件
- `BUILD_CLI=ON`: 构建 bitcoin-cli 可执行文件
- `BUILD_GUI=OFF`: 构建 bitcoin-qt 可执行文件
- `ENABLE_WALLET=ON`: 启用钱包功能
- `BUILD_TESTS=ON`: 构建测试
- `WITH_ZMQ=OFF`: 启用 ZMQ 通知
- `WERROR=OFF`: 将编译器警告视为错误
- `SANITIZERS`: 启用内存/线程/未定义行为检测器

### 运行 Bitcoin Core

- **守护进程模式**: `bitcoind`
- **命令行模式**: `bitcoin-cli getinfo`
- **测试网络**: `bitcoind -testnet4`
- **回归测试模式**: `bitcoind -regtest`

## 开发提示

### 调试

- 使用 `-debug` 参数启用调试日志
- 日志文件位于数据目录的 `debug.log`
- 使用 `-regtest` 进行本地测试
- 使用 `-debug=1` 或 `-debug=category_name` 启用特定日志类别

### 代码审查

- 所有代码变更都需要同行审查
- 重点关注共识关键代码
- 检查是否遵循编码风格和最佳实践
- 确保有适当的测试覆盖

### 贡献流程

1. Fork 仓库
2. 创建主题分支
3. 提交补丁
4. 创建拉取请求
5. 接受同行审查
6. 合并到主分支

## 安全考虑

- 代码库具有严格的安全要求，特别是共识关键代码
- 使用各种检测器 (sanitizers) 检测内存错误、线程竞态条件和未定义行为
- 实施锁定顺序检查以避免死锁
- 有专门的安全和漏洞修复流程

## 学习资源

- 查看 `doc/developer-notes.md` 了解详细的开发指南
- 参考 `CONTRIBUTING.md` 了解贡献流程
- 阅读 `doc/` 目录中的文档
- 参与 IRC 频道 `#bitcoin-core-dev` 上的讨论