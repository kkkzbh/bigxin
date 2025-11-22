-- friend_requests 表：存放好友申请及其处理状态。
-- 仅保留必要字段，满足“新的朋友”列表和申请状态展示。
CREATE TABLE IF NOT EXISTS friend_requests (
    -- 主键，自增申请 ID。
    id           BIGSERIAL PRIMARY KEY,
    -- 申请发起者。
    from_user_id BIGINT NOT NULL REFERENCES users(id),
    -- 申请接收者（在“新的朋友”列表中看到的人）。
    to_user_id   BIGINT NOT NULL REFERENCES users(id),
    -- 状态：PENDING / ACCEPTED / REJECTED / CANCELED。
    status       TEXT   NOT NULL DEFAULT 'PENDING',
    -- 申请来源，例如 search_account / group 等。
    source       TEXT,
    -- 打招呼信息（前端可选填）。
    hello_msg    TEXT,
    -- 创建时间。
    created_at   TIMESTAMPTZ NOT NULL DEFAULT now(),
    -- 处理时间（通过 / 拒绝 / 撤回时更新）。
    handled_at   TIMESTAMPTZ
);

CREATE INDEX IF NOT EXISTS friend_requests_to_status_idx
    ON friend_requests (to_user_id, status);

CREATE INDEX IF NOT EXISTS friend_requests_from_status_idx
    ON friend_requests (from_user_id, status);

