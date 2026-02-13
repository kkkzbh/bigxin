# bigxin

`bigxin` 是一个基于 C++ 的即时通讯（IM）项目，包含：
- `server`：基于 Boost.Asio 协程的 TCP 聊天服务端
- `client`：基于 Qt Quick/QML 的桌面客户端
- `benchmark`：用于连接与消息吞吐压测的独立工具

项目当前协议为**文本行协议 + JSON 负载**，详细定义见 `doc/protocol.md`。
说明：仓库内部分代码/目标名仍使用历史命名 `JuXin`，不影响构建与运行。

## 目录

- [项目特性](#项目特性)
- [技术栈](#技术栈)
- [项目结构](#项目结构)
- [环境要求](#环境要求)
- [快速开始](#快速开始)
- [数据库初始化](#数据库初始化)
- [运行方式](#运行方式)
- [压测工具](#压测工具)
- [协议说明](#协议说明)
- [常见问题](#常见问题)
- [开发说明](#开发说明)

## 项目特性

- 用户注册 / 登录
- 会话列表与历史消息拉取
- 单聊与群聊
- 好友体系（搜索、申请、通过、删除）
- 群组管理（创建群、入群申请、群成员管理、管理员设置、禁言）
- 消息已读、撤回、点赞/点踩反应
- 头像上传与拉取、本地消息缓存
- 基于 `benchmark` 的连接/消息压测

## 技术栈

- C++26
- CMake（顶层 `CMakeLists.txt`）
- Boost.Asio + Boost.MySQL
- Qt 6（Quick / QuickControls2 / Network）
- nlohmann-json
- stdexec（通过 CPM 拉取）

## 项目结构

```text
.
├── CMakeLists.txt              # 顶层构建配置
├── include/                    # 公共头文件
├── src/
│   ├── server/                 # 服务端实现
│   ├── client/                 # Qt 客户端与 QML 界面
│   ├── database/               # 数据访问层
│   └── benchmark/              # 压测工具
├── doc/
│   ├── protocol.md             # 协议文档
│   └── BUILD_GUIDE.md          # 跨平台打包参考
└── src/database/sql/           # 数据库建表与迁移脚本
```

## 环境要求

- CMake `>= 4.0.2`
- 支持 C++26 的编译器（GCC/Clang/MSVC 新版本）
- Qt `>= 6.5`（需包含 `Quick`, `QuickControls2`, `Network` 组件）
- MySQL `8.x`
- vcpkg（推荐）与以下依赖：
  - `boost-asio`
  - `boost-mysql`
  - `nlohmann-json`

## 快速开始

### 1. 获取代码

```bash
git clone <your-repo-url> bigxin
cd bigxin
```

### 2. 安装依赖（vcpkg 示例）

```bash
# 按你的 triplet 调整（如 x64-linux / x64-linux-static / x64-windows 等）
vcpkg install boost-asio boost-mysql nlohmann-json
```

### 3. 配置与构建

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_TOOLCHAIN_FILE=<vcpkg-root>/scripts/buildsystems/vcpkg.cmake

cmake --build build -j
```

构建后主要产物：
- `build/src/server`
- `build/src/client`（Windows 下通常是 `client.exe`）
- `build/src/benchmark/benchmark`

## 数据库初始化

服务端默认数据库连接配置（见 `include/database/connection.h`）：
- host: `127.0.0.1`
- port: `3307`
- user: `kkkzbh`
- password: `kkkzbh`
- database: `chatdb`

请先创建数据库：

```sql
CREATE DATABASE chatdb CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
```

然后按顺序执行 SQL 脚本（顺序很重要，涉及外键）：

```bash
mysql -h 127.0.0.1 -P 3307 -u kkkzbh -p chatdb < src/database/sql/users.sql
mysql -h 127.0.0.1 -P 3307 -u kkkzbh -p chatdb < src/database/sql/conversations.sql
mysql -h 127.0.0.1 -P 3307 -u kkkzbh -p chatdb < src/database/sql/conversation_members.sql
mysql -h 127.0.0.1 -P 3307 -u kkkzbh -p chatdb < src/database/sql/single_conversations.sql
mysql -h 127.0.0.1 -P 3307 -u kkkzbh -p chatdb < src/database/sql/friends.sql
mysql -h 127.0.0.1 -P 3307 -u kkkzbh -p chatdb < src/database/sql/friend_requests.sql
mysql -h 127.0.0.1 -P 3307 -u kkkzbh -p chatdb < src/database/sql/group_join_requests.sql
mysql -h 127.0.0.1 -P 3307 -u kkkzbh -p chatdb < src/database/sql/messages.sql
mysql -h 127.0.0.1 -P 3307 -u kkkzbh -p chatdb < src/database/sql/conversation_sequences.sql
mysql -h 127.0.0.1 -P 3307 -u kkkzbh -p chatdb < src/database/sql/migration_002_message_reactions.sql
```

如果你的数据库配置不同，请调整 `include/database/connection.h` 的默认值，或在服务启动时注入自定义配置。

## 运行方式

### 启动服务端

```bash
# 默认端口 5555
./build/src/server

# 指定端口
./build/src/server 5555
```

### 启动客户端

```bash
./build/src/client --host 127.0.0.1 --port 5555
```

客户端参数：
- `-H, --host`：服务器地址（默认 `127.0.0.1`）
- `-P, --port`：服务器端口（默认 `5555`）
- `-h, --help`：帮助
- `-v, --version`：版本

## 压测工具

`benchmark` 支持 `setup / connect / message / world / full` 模式。

常用流程：

```bash
# 1) 初始化测试账号与群组
./build/src/benchmark/benchmark setup --prefix test_

# 2) 连接压测
./build/src/benchmark/benchmark connect --prefix test_ --accounts 200 --connect-window 4

# 3) 消息压测
./build/src/benchmark/benchmark message --prefix test_ --duration 60
```

`setup` 会在当前目录生成 `<prefix>benchmark_data.json`，供后续模式复用。

## 协议说明

- 传输：TCP 长连接
- 编码：UTF-8
- 帧格式：`COMMAND:{"json":"payload"}\n`

示例：

```text
SEND_MSG:{"conversationId":"1","senderId":"2","msgType":"TEXT","content":"hello"}\n
```

详见：`doc/protocol.md`

## 常见问题

### 1. 客户端连不上服务端

- 确认服务端已启动并监听正确端口
- 检查客户端 `--host` / `--port` 是否一致
- 检查防火墙与端口占用

### 2. 启动后登录/注册报数据库错误

- 确认 MySQL 已启动，地址与端口与 `include/database/connection.h` 一致
- 确认 `chatdb` 已创建，且 SQL 初始化已完成

### 3. Qt 模块找不到

- 确认 CMake 能找到 Qt6（必要时设置 `CMAKE_PREFIX_PATH`）

## 开发说明

- 构建入口：`CMakeLists.txt` 与 `src/CMakeLists.txt`
- 协议调试：可用 `nc`/`telnet` 发送文本协议行验证服务端行为
- 打包参考：`doc/BUILD_GUIDE.md`

## 安全提示

当前客户端 QML 中包含 AI 接口调用逻辑；如果用于公开仓库或生产环境，请将密钥改为安全注入方式（环境变量/服务端代理），不要将密钥明文提交。
