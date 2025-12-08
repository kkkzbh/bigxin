-- 添加 is_recalled 字段到 messages 表
ALTER TABLE messages ADD COLUMN is_recalled BOOLEAN NOT NULL DEFAULT FALSE;

-- message_reactions 表：存储消息的点赞/踩反应
CREATE TABLE IF NOT EXISTS message_reactions (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    message_id BIGINT NOT NULL,
    user_id BIGINT NOT NULL,
    reaction_type VARCHAR(16) NOT NULL,  -- 'LIKE' 或 'DISLIKE'
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uk_message_user_reaction (message_id, user_id),  -- 每个用户对每条消息只能有一个反应
    INDEX idx_message_reactions_msg (message_id),
    CONSTRAINT fk_reaction_msg FOREIGN KEY (message_id) REFERENCES messages(id) ON DELETE CASCADE,
    CONSTRAINT fk_reaction_user FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
