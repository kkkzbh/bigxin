-- friend_requests 表：存放好友申请及其处理状态。
CREATE TABLE IF NOT EXISTS friend_requests (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    from_user_id BIGINT NOT NULL,
    to_user_id BIGINT NOT NULL,
    status VARCHAR(16) NOT NULL DEFAULT 'PENDING',
    source VARCHAR(64),
    hello_msg TEXT,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    handled_at TIMESTAMP NULL DEFAULT NULL,
    INDEX idx_friend_requests_to_status (to_user_id, status, created_at DESC),
    INDEX idx_friend_requests_from_status (from_user_id, status),
    INDEX idx_friend_requests_pending_from_to (from_user_id, to_user_id),
    INDEX idx_friend_requests_pending_to_from (to_user_id, from_user_id),
    CONSTRAINT fk_fr_from FOREIGN KEY (from_user_id) REFERENCES users(id) ON DELETE CASCADE,
    CONSTRAINT fk_fr_to FOREIGN KEY (to_user_id) REFERENCES users(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
