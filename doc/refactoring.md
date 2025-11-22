# LoginBackend 重构说明

## 重构概述

原 `LoginBackend` 类已从单一的 1600+ 行巨大头文件拆分为模块化的架构，提升了代码的可维护性、可测试性和可复用性。

## 架构设计

### 模块划分

#### 1. NetworkManager (网络通信层)
- **位置**: `include/network_manager.h` 和 `src/client/backend/network_manager.cpp`
- **职责**: 
  - 封装 QTcpSocket，管理 TCP 连接
  - 处理断线重连逻辑
  - 文本协议的收发（"COMMAND:{...}\n" 格式）
- **接口**:
  - `connectToServer(host, port)` - 连接服务器
  - `sendCommand(command, payload)` - 发送命令
  - `commandReceived(command, payload)` - 信号：收到服务器命令

#### 2. MessageCache (消息缓存层)
- **位置**: `include/message_cache.h` 和 `src/client/backend/message_cache.cpp`
- **职责**:
  - 管理本地消息历史缓存
  - 按用户和会话组织缓存文件
  - 提供消息的读写接口
- **接口**:
  - `setUserId(userId)` - 设置当前用户
  - `writeMessages(conversationId, messages)` - 写入消息
  - `appendMessage(...)` - 追加单条消息
  - `loadMessages(conversationId)` - 加载缓存消息

#### 3. ProtocolHandler (协议处理层)
- **位置**: `include/protocol_handler.h` 和 `src/client/backend/protocol_handler.cpp`
- **职责**:
  - 接收 NetworkManager 的命令并分发处理
  - 实现所有 `handle*Response()` 方法
  - 维护会话 seq 状态
  - 与 MessageCache 协作管理缓存
- **主要信号**:
  - `loginSucceeded` - 登录成功
  - `messageReceived` - 收到消息
  - `conversationsReset` - 会话列表更新
  - `friendsReset` - 好友列表更新
  - 等等...

#### 4. LoginBackend (门面层)
- **位置**: `include/login.backend.h` 和 `src/client/backend/login_backend.cpp`
- **职责**:
  - 保留 Q_PROPERTY 和 Q_INVOKABLE 接口以维持 QML 兼容性
  - 维护 UI 状态（busy、errorMessage、userId、displayName）
  - 委托底层模块执行具体操作
  - 转发信号到 QML

## 数据流

```
QML
 ↓ (Q_INVOKABLE 调用)
LoginBackend
 ↓ (委托)
NetworkManager → 服务器
 ↓ (commandReceived 信号)
ProtocolHandler
 ↓ (处理响应)
MessageCache (可选，用于缓存)
 ↓ (发出信号)
LoginBackend
 ↓ (转发信号)
QML
```

## 优势

### 1. 职责清晰
每个模块只负责一个明确的功能领域，符合单一职责原则。

### 2. 易于测试
各模块可以独立进行单元测试：
- NetworkManager 可以模拟 socket 行为测试
- MessageCache 可以测试文件 I/O
- ProtocolHandler 可以独立测试协议解析逻辑

### 3. 可复用性
- NetworkManager 可用于其他需要文本协议通信的场景
- MessageCache 可扩展为通用的本地数据缓存模块
- ProtocolHandler 可根据需要进一步拆分为专项处理器

### 4. 易于维护
- 修改网络层逻辑时无需触及协议处理代码
- 修改缓存策略时无需修改网络通信代码
- 添加新协议时只需在 ProtocolHandler 中增加处理方法

## QML 兼容性

重构后 QML 代码**无需任何修改**，所有现有接口保持不变：
- 所有 Q_PROPERTY 保持原样
- 所有 Q_INVOKABLE 方法保持原样
- 所有信号保持原样

## 文件结构

```
chat/
├── include/
│   ├── login.backend.h        (轻量门面，仅声明)
│   ├── network_manager.h      (网络通信层)
│   ├── message_cache.h        (消息缓存层)
│   └── protocol_handler.h     (协议处理层)
└── src/
    └── client/
        ├── client.cpp
        └── backend/
            ├── login_backend.cpp
            ├── network_manager.cpp
            ├── message_cache.cpp
            └── protocol_handler.cpp
```

## 后续扩展建议

1. **进一步拆分 ProtocolHandler**:
   - `FriendManager` - 好友相关协议处理
   - `ConversationManager` - 会话相关协议处理
   - `MessageManager` - 消息相关协议处理

2. **引入数据模型层**:
   - 为会话列表、好友列表等创建 QAbstractListModel
   - 在 C++ 侧提供更好的数据管理

3. **添加单元测试**:
   - 为各模块编写完整的单元测试
   - 使用 mock 对象测试模块间交互

4. **日志系统**:
   - 在各模块中添加结构化日志
   - 方便调试和问题追踪
