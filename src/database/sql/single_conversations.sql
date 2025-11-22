-- single_conversations 表：记录两两用户之间的单聊会话唯一对应关系。
-- 一行代表一对用户之间的单聊会话，用于防止重复创建 SINGLE 会话。
CREATE TABLE IF NOT EXISTS single_conversations (
    -- 较小的用户 ID，保证 (user1_id, user2_id) 有唯一排序。
    user1_id       BIGINT NOT NULL REFERENCES users(id),
    -- 较大的用户 ID。
    user2_id       BIGINT NOT NULL REFERENCES users(id),
    -- 对应的会话 ID，指向 conversations 表中 type='SINGLE' 的记录。
    conversation_id BIGINT NOT NULL REFERENCES conversations(id),
    -- 约束：同一对用户只能有一条单聊记录。
    CONSTRAINT single_conversations_pair_unique UNIQUE (user1_id, user2_id)
);

