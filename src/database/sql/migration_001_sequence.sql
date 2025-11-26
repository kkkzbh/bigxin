-- 迁移脚本：为 MySQL 消息序列号提供 per-conversation 计数器
-- 适用版本：MySQL 8.4+

CREATE TABLE IF NOT EXISTS conversation_sequences (
    conversation_id BIGINT NOT NULL PRIMARY KEY,
    next_seq BIGINT NOT NULL,
    CONSTRAINT fk_conv_seq_conv FOREIGN KEY (conversation_id) REFERENCES conversations(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 可选：根据现有 messages 表回填 next_seq
INSERT INTO conversation_sequences (conversation_id, next_seq)
SELECT m.conversation_id, COALESCE(MAX(m.seq), 0) + 1
FROM messages m
GROUP BY m.conversation_id
ON DUPLICATE KEY UPDATE next_seq = VALUES(next_seq);
