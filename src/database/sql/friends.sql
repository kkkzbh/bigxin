-- friends 表：存放已经建立的双向好友关系。
-- 为了便于查询“我的好友列表”，一对好友存为两行：(A,B) 和 (B,A)。
CREATE TABLE IF NOT EXISTS friends (
    -- 当前用户 ID。
    user_id        BIGINT NOT NULL REFERENCES users(id),
    -- 好友用户 ID。
    friend_user_id BIGINT NOT NULL REFERENCES users(id),
    -- 建立好友关系时间。
    created_at     TIMESTAMPTZ NOT NULL DEFAULT now(),
    PRIMARY KEY (user_id, friend_user_id)
);

