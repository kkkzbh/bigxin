-- group_join_requests 表：存放入群申请及其处理状态。
CREATE TABLE IF NOT EXISTS group_join_requests (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    from_user_id BIGINT NOT NULL,
    group_id BIGINT NOT NULL,
    status VARCHAR(16) NOT NULL DEFAULT 'PENDING',
    hello_msg TEXT,
    handler_user_id BIGINT NULL,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    handled_at TIMESTAMP NULL DEFAULT NULL,
    INDEX idx_gjr_group_status (group_id, status, created_at DESC),
    INDEX idx_gjr_from_status (from_user_id, status),
    INDEX idx_gjr_pending (group_id, from_user_id, status),
    CONSTRAINT fk_gjr_from FOREIGN KEY (from_user_id) REFERENCES users(id) ON DELETE CASCADE,
    CONSTRAINT fk_gjr_group FOREIGN KEY (group_id) REFERENCES conversations(id) ON DELETE CASCADE,
    CONSTRAINT fk_gjr_handler FOREIGN KEY (handler_user_id) REFERENCES users(id) ON DELETE SET NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
