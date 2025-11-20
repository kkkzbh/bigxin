# 聊天系统文本协议设计文档

版本：v0.1  
适用范围：后端 C++ (Qt + Asio)，前端 QML  

## 1. 设计目标

- 基于纯文本，便于调试和课堂展示，满足“自定义协议：COMMAND:param1,param2…\n”的要求。
- 协议结构清晰、可扩展，适合 IM/微信类应用（群聊优先，未来扩展单聊）。
- 支持可靠发送（去重、确认）、在线消息推送、离线/历史消息拉取。
- 协议形式接近工业实践，便于未来迁移到 JSON over HTTP 或 Protobuf。

## 2. 传输层与基础约定

- 传输层：基于 TCP 长连接（Asio 实现）。
- 字符编码：UTF-8。
- 消息边界：**一条消息是一行文本**，以换行符 `\n` 结束。
- 基本格式：

```text
<COMMAND>:<JSON_OBJECT>\n
```

示例：

```text
SEND_MSG:{"conversationId":"grp-123","senderId":"u_001","clientMsgId":"cli-1","msgType":"TEXT","content":"大家好"}\n
```

解析流程（服务器或客户端）：

1. 使用 `async_read_until(socket, buffer, '\n')` 读取一行。
2. 从行内找到第一个字符 `:`，其左侧为命令名 `COMMAND`，右侧到换行符前为 JSON 串。
3. 将 JSON 串交给 JSON 库（如 Qt 的 QJsonDocument/QJsonObject）解析。
4. 根据 `COMMAND` 分发到对应处理逻辑。

## 3. 通用字段约定

为方便扩展，不同命令的 JSON 对象中经常会复用以下字段：

- `userId`：用户 ID，字符串。
- `conversationId`：会话 ID，字符串。
  - 群聊：群 ID，例如 `"grp-123"`。
  - 单聊（未来）：可以约定为 `"single-u001-u002"` 或由服务器生成独立会话 ID。
- `conversationType`：会话类型，字符串枚举。
  - 当前：`"GROUP"`。
  - 未来扩展：`"SINGLE"`。
- `clientMsgId`：客户端生成的消息 ID，用于发送重试时的幂等去重。
- `serverMsgId`：服务器生成的消息 ID，全局唯一。
- `msgType`：消息类型，字符串枚举。
  - 当前：`"TEXT"`。
  - 未来扩展：`"IMAGE"`, `"FILE"`, `"AUDIO"` 等。
- `serverTimeMs`：服务器写入时间戳，毫秒级 Unix 时间。
- `seq`：会话内递增消息序号，用于排序、历史拉取和游标控制。
- `ok`：布尔值，表示请求是否成功。
- `errorCode`：错误码字符串，例如 `"LOGIN_FAILED"`, `"INVALID_PARAM"`。
- `errorMsg`：错误信息，主要用于调试和展示。

## 4. 命令一览

- 登录鉴权：
  - `LOGIN` (C → S)
  - `LOGIN_RESP` (S → C)
- 消息发送与确认：
  - `SEND_MSG` (C → S)
  - `SEND_ACK` (S → C)
- 消息推送与送达确认：
  - `MSG_PUSH` (S → C)
  - `MSG_ACK` (C → S)
- 历史/离线消息：
  - `HISTORY_REQ` (C → S)
  - `HISTORY_RESP` (S → C)
- 会话列表：
  - `CONV_LIST_REQ` (C → S)
  - `CONV_LIST_RESP` (S → C)
- 用户资料：
  - `PROFILE_UPDATE` (C → S)
  - `PROFILE_UPDATE_RESP` (S → C)
- 好友 / 通讯录：
  - `FRIEND_LIST_REQ` / `FRIEND_LIST_RESP`
  - `FRIEND_SEARCH_REQ` / `FRIEND_SEARCH_RESP`
  - `FRIEND_ADD_REQ` / `FRIEND_ADD_RESP`
  - `FRIEND_REQ_LIST_REQ` / `FRIEND_REQ_LIST_RESP`
  - `FRIEND_ACCEPT_REQ` / `FRIEND_ACCEPT_RESP`
  - `OPEN_SINGLE_CONV_REQ` / `OPEN_SINGLE_CONV_RESP`
- 群聊：
  - `CREATE_GROUP_REQ` / `CREATE_GROUP_RESP`
- 心跳保活：
  - `PING` (C ↔ S)
  - `PONG` (C ↔ S)

下面详细定义各命令。

## 5. 登录 / 注册与鉴权

### 5.1 LOGIN（C → S）

客户端使用“账号 + 密码”请求登录。

格式：

```text
LOGIN:{
  "account":  "your_account",
  "password": "plain_or_hash"
}\n
```

字段：

- `account`：登录账号（可以是微信号 / 邮箱 / 手机号，后端只看唯一性）。
- `password`：密码，当前可以先用明文，后续再升级为哈希。

### 5.2 LOGIN_RESP（S → C）

服务器返回登录结果。

格式（成功）：

```text
LOGIN_RESP:{
  "ok": true,
  "userId": "1",
  "displayName": "微信用户123456"
}\n
```

格式（失败）：

```text
LOGIN_RESP:{
  "ok": false,
  "errorCode": "LOGIN_FAILED",
  "errorMsg": "账号不存在或密码错误"
}\n
```

字段：

- `ok`：登录是否成功。
- `userId`：成功时返回当前用户 ID（字符串形式，例如 `"1"`）。
- `displayName`：成功时返回当前用户昵称，用于聊天界面展示。
- `errorCode`：失败时的错误码。
- `errorMsg`：失败原因描述。

### 5.3 REGISTER（C → S）

客户端注册新账号。

前端只填写账号 + 密码 + 确认密码，昵称由后端生成一个随机值，例如“微信用户123456”。

格式：

```text
REGISTER:{
  "account":         "your_account",
  "password":        "plain_or_hash",
  "confirmPassword": "plain_or_hash"
}\n
```

字段：

- `account`：注册账号，需在系统中唯一。
- `password`：密码。
- `confirmPassword`：确认密码（后端只检查与 `password` 是否一致）。

### 5.4 REGISTER_RESP（S → C）

服务器返回注册结果。

格式（成功）：

```text
REGISTER_RESP:{
  "ok": true,
  "userId": "1",
  "displayName": "微信用户123456"
}\n
```

格式（失败）：

```text
REGISTER_RESP:{
  "ok": false,
  "errorCode": "ACCOUNT_EXISTS",
  "errorMsg": "账号已存在"
}\n
```

字段：

- `ok`：是否注册成功。
- `userId`：成功时返回新用户 ID。
- `displayName`：成功时返回随机生成的初始昵称。
- `errorCode`：失败时错误码，如 `"ACCOUNT_EXISTS"`。
- `errorMsg`：失败时错误信息。

### 5.5 PROFILE_UPDATE（C → S）

客户端修改当前登录用户的个人资料（目前仅支持昵称）。

格式：

```text
PROFILE_UPDATE:{
  "displayName": "新的昵称"
}\n
```

字段：

- `displayName`：新的昵称，去除首尾空白后不能为空，长度建议限制在 64 字节以内。

### 5.6 PROFILE_UPDATE_RESP（S → C）

服务器返回资料更新结果。

格式（成功）：

```text
PROFILE_UPDATE_RESP:{
  "ok": true,
  "displayName": "新的昵称"
}\n
```

格式（失败）：

```text
PROFILE_UPDATE_RESP:{
  "ok": false,
  "errorCode": "INVALID_PARAM" | "NOT_AUTHENTICATED" | "SERVER_ERROR",
  "errorMsg": "错误信息"
}\n
```

字段：

- `ok`：是否更新成功。
- `displayName`：成功时返回最新昵称。
- `errorCode`：失败时错误码。
- `errorMsg`：失败时错误信息。

## 6. 消息发送与确认

### 6.1 SEND_MSG（C → S）

客户端发送一条聊天消息（群聊为主，未来可扩展单聊）。

格式：

```text
SEND_MSG:{
  "conversationId":   "grp-123",
  "conversationType": "GROUP",
  "senderId":         "u_001",
  "clientMsgId":      "cli-1731312345123",

  "msgType": "TEXT",
  "content": "大家好"  // 文本内容，UTF-8，允许中文和 emoji
}\n
```

字段：

- `conversationId`：会话 ID（当前为群 ID）。
- `conversationType`：会话类型，当前为 `"GROUP"`。
- `senderId`：发送者用户 ID。
- `clientMsgId`：客户端生成的本地消息 ID，用于重试时幂等去重。
- `msgType`：消息类型，当前固定为 `"TEXT"`。
- `content`：消息文本内容。

客户端发送后：

- 若在超时时间内未收到 `SEND_ACK`，可以使用**同一个** `clientMsgId` 重发一次 `SEND_MSG`；
- 服务器端应使用 `(conversationId, clientMsgId)` 做幂等判断，如果已经处理过，直接返回之前的结果。

### 6.2 SEND_ACK（S → C）

服务器确认已接收并持久化该消息，返回消息的服务器 ID 和序号等信息。

格式：

```text
SEND_ACK:{
  "clientMsgId": "cli-1731312345123",
  "serverMsgId": "srv-8888",
  "serverTimeMs": 1731312346000,
  "seq": 101
}\n
```

字段：

- `clientMsgId`：对应请求中的 `clientMsgId`。
- `serverMsgId`：服务器为该消息生成的全局唯一 ID。
- `serverTimeMs`：服务器写入消息的时间戳（毫秒）。
- `seq`：该消息在此会话中的递增序号。

客户端收到 `SEND_ACK` 后，可以将该条消息从“发送中”状态更新为“已发送成功”。

## 7. 消息推送与送达确认

### 7.1 MSG_PUSH（S → C）

服务器向会话中所有在线成员推送新消息。

格式：

```text
MSG_PUSH:{
  "conversationId":   "grp-123",
  "conversationType": "GROUP",

  "serverMsgId": "srv-8888",
  "senderId":    "u_001",
  "senderDisplayName": "张三",
  "msgType":     "TEXT",
  "serverTimeMs": 1731312346000,
  "seq":          101,

  "content": "大家好"
}\n
```

字段：

- `conversationId` / `conversationType`：会话信息。
- `serverMsgId`：消息的服务器 ID。
- `senderId`：发送者 ID。
- `senderDisplayName`：发送者昵称（已规范化、去首尾空白）。
- `msgType`：消息类型。
- `serverTimeMs`：消息在服务器写入时间。
- `seq`：会话内消息序号。
- `content`：消息内容（类型为 TEXT）。

### 7.2 MSG_ACK（C → S）

客户端收到 `MSG_PUSH` 后，回送一个送达确认，便于服务器记录哪些客户端已成功收到消息。

格式：

```text
MSG_ACK:{
  "conversationId": "grp-123",
  "serverMsgId":   "srv-8888"
}\n
```

字段：

- `conversationId`：会话 ID。
- `serverMsgId`：被确认的消息 ID。

服务器可据此更新“消息送达状态”，为将来实现多端同步、已读回执等功能打基础。

## 8. 历史 / 离线消息

### 8.1 HISTORY_REQ（C → S）

客户端按会话拉取历史消息或离线消息。

格式：

```text
HISTORY_REQ:{
  "conversationId": "grp-123",
  "beforeSeq": 0,
  "limit": 20
}\n
```

字段：

- `conversationId`：会话 ID。
- `beforeSeq`：
  - 当为 `0` 或缺省时，表示从最新消息开始向前拉取；
  - 当为某个正整数时，表示拉取 `seq < beforeSeq` 的消息。
- `limit`：最大返回条数，用于分页。

### 8.2 HISTORY_RESP（S → C）

服务器返回一批历史消息。

格式：

```text
HISTORY_RESP:{
  "conversationId": "grp-123",
  "messages": [
    {
      "serverMsgId":  "srv-8881",
      "senderId":     "u_002",
      "senderDisplayName": "李四",
      "msgType":      "TEXT",
      "serverTimeMs": 1731312300000,
      "seq":          99,
      "content":      "早上好"
    },
    {
      "serverMsgId":  "srv-8882",
      "senderId":     "u_003",
      "senderDisplayName": "王五",
      "msgType":      "TEXT",
      "serverTimeMs": 1731312310000,
      "seq":          100,
      "content":      "中午好"
    }
  ],
  "hasMore": true,
  "nextBeforeSeq": 99
}\n
```

字段：

- `messages`：消息数组，每个元素结构与 `MSG_PUSH` 中的消息体基本一致。
- `hasMore`：是否还有更早的消息可以继续拉取。
- `nextBeforeSeq`：客户端下次请求时可作为 `beforeSeq` 使用。

## 9. 心跳保活

为保持 TCP 长连接不被中间设备关闭，可以周期性发送心跳。

### 9.1 PING（C ↔ S）

格式：

```text
PING:{}\n
```

### 9.2 PONG（C ↔ S）

格式：

```text
PONG:{}\n
```

客户端定期发送 `PING`，服务器收到后立即返回 `PONG`。双方都可以根据心跳超时时间判断对端是否存活。

## 10. 错误处理（可选强化）

对于需要单独错误信息的场景，可以统一使用 `*_RESP` 中的 `ok / errorCode / errorMsg` 字段。  
如需更加统一，也可以增加一个通用错误命令：

```text
ERROR:{
  "inCommand": "SEND_MSG",
  "errorCode": "INVALID_PARAM",
  "errorMsg":  "conversationId is empty"
}\n
```

客户端在收到 `ERROR` 时，可根据 `inCommand` 和 `errorCode` 决定具体提示和恢复策略。

## 11. 未来扩展方向

本协议已满足：

- 多人群聊消息发送与推送；
- 消息可靠性（发送确认 + 送达确认 + 重发幂等）；
- 历史/离线消息拉取；
- 长连接心跳保活。

未来可以在保持整体结构不变的前提下扩展：

- 支持单聊：在所有消息中增加/使用 `"conversationType":"SINGLE"`。
- 丰富消息类型：为 `IMAGE`、`FILE`、`AUDIO` 等定义包含 URL、大小等属性的 JSON 结构。
- 支持 `atList`、`quoteMsgId`、`ext` 等高级字段。
- 将 JSON 对象结构一一映射到 Protobuf，保持字段含义一致，平滑迁移到二进制协议。
