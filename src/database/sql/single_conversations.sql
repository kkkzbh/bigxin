-- single_conversations 表：记录两两用户之间的单聊会话唯一对应关系。
CREATE TABLE IF NOT EXISTS single_conversations (
    user1_id BIGINT NOT NULL,
    user2_id BIGINT NOT NULL,
    conversation_id BIGINT NOT NULL,
    UNIQUE KEY uk_single_pair (user1_id, user2_id),
    CONSTRAINT fk_sc_conv FOREIGN KEY (conversation_id) REFERENCES conversations(id) ON DELETE CASCADE,
    CONSTRAINT fk_sc_user1 FOREIGN KEY (user1_id) REFERENCES users(id) ON DELETE CASCADE,
    CONSTRAINT fk_sc_user2 FOREIGN KEY (user2_id) REFERENCES users(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
