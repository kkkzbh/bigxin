-- messages 表：存放所有会话内的聊天消息。
-- 一行代表一条消息，支持按会话和序号/时间查询历史。
CREATE TABLE IF NOT EXISTS messages (
    -- 消息主键 ID，作为 serverMsgId。
    id              BIGSERIAL PRIMARY KEY,
    -- 所属会话 ID。
    conversation_id BIGINT  NOT NULL REFERENCES conversations(id),
    -- 发送者用户 ID。
    sender_id       BIGINT  NOT NULL REFERENCES users(id),
    -- 会话内递增的消息序号，用于排序和游标翻页。
    seq             BIGINT  NOT NULL,
    -- 消息类型：'TEXT' / 将来可扩展 'IMAGE' 等。
    msg_type        TEXT    NOT NULL,
    -- 消息内容，当前为文本，将来也可以存 JSON。
    content         TEXT    NOT NULL,
    -- 服务器时间戳（毫秒），对应协议里的 serverTimeMs。
    server_time_ms  BIGINT  NOT NULL,
    -- 插入时间。
    created_at      TIMESTAMPTZ NOT NULL DEFAULT now()
);

-- 唯一索引：保证同一会话内 seq 不重复。
CREATE UNIQUE INDEX IF NOT EXISTS messages_conv_seq_idx
    ON messages (conversation_id, seq);

-- 普通索引：按会话和物理顺序加速查询。
CREATE INDEX IF NOT EXISTS messages_conv_time_idx
    ON messages (conversation_id, id);

