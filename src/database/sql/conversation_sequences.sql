-- conversation_sequences 表：为每个会话维护独立的消息序列号。
CREATE TABLE IF NOT EXISTS conversation_sequences (
    conversation_id BIGINT NOT NULL PRIMARY KEY,
    next_seq BIGINT NOT NULL,
    CONSTRAINT fk_conv_seq_conv FOREIGN KEY (conversation_id) REFERENCES conversations(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
