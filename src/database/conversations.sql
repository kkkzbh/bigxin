-- conversations 表：存放会话（群聊 / 单聊）的元信息。
-- 一行代表一个会话，群聊和单聊统一放在这里。
CREATE TABLE IF NOT EXISTS conversations (
    -- 会话 ID，自增主键。
    id            BIGSERIAL PRIMARY KEY,
    -- 会话类型：'GROUP' / 将来可扩展 'SINGLE'。
    type          TEXT NOT NULL,
    -- 会话名称：群名称或单聊时的显示名称。
    name          TEXT NOT NULL,
    -- 会话创建者（群主），单聊时可为空。
    owner_user_id BIGINT REFERENCES users(id),
    -- 创建时间。
    created_at    TIMESTAMPTZ NOT NULL DEFAULT now()
);

-- 唯一索引：保证同一类型和名称的会话只有一条记录。
CREATE UNIQUE INDEX IF NOT EXISTS conversations_type_name_idx
    ON conversations (type, name);

-- 初始化默认“世界”会话，所有用户共享的群聊。
INSERT INTO conversations (type, name, owner_user_id)
VALUES ('GROUP', '世界', NULL)
ON CONFLICT DO NOTHING;
