-- conversations 表：存放会话（群聊 / 单聊）的元信息。
CREATE TABLE IF NOT EXISTS conversations (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    type VARCHAR(16) NOT NULL,
    name VARCHAR(255) NOT NULL,
    owner_user_id BIGINT NULL,
    avatar_path VARCHAR(512) DEFAULT NULL,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_conversations_owner(owner_user_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- GROUP 会话名称可重复，故不再加唯一约束。

-- 初始化默认“世界”会话。
INSERT IGNORE INTO conversations (type, name, owner_user_id)
VALUES ('GROUP', '世界', NULL);
