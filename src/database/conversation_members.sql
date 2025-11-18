-- conversation_members 表：记录会话成员关系。
-- 一行代表某个用户属于某个会话，可用于权限控制和未读计算。
CREATE TABLE IF NOT EXISTS conversation_members (
    -- 会话 ID。
    conversation_id BIGINT NOT NULL REFERENCES conversations(id),
    -- 成员用户 ID。
    user_id         BIGINT NOT NULL REFERENCES users(id),
    -- 角色：'OWNER' / 'MEMBER' 等。
    role            TEXT   NOT NULL DEFAULT 'MEMBER',
    -- 加入会话时间。
    joined_at       TIMESTAMPTZ NOT NULL DEFAULT now(),
    -- 该用户在此会话中最后读到的消息序号，用于计算未读。
    last_read_seq   BIGINT NOT NULL DEFAULT 0,
    PRIMARY KEY (conversation_id, user_id)
);
