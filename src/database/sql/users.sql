-- users 表：存放用户账号、密码及基础信息。
-- 一行代表一个用户，用于登录鉴权和展示昵称。
CREATE TABLE IF NOT EXISTS users (
    -- 主键，自增用户 ID。
    id            BIGSERIAL PRIMARY KEY,
    -- 登录账号（微信号 / 邮箱 / 手机等），全局唯一。
    account       TEXT NOT NULL UNIQUE,
    -- 密码哈希（当前存明文，后续可替换为哈希）。
    password_hash TEXT NOT NULL,
    -- 昵称，例如“微信用户123456”。
    display_name  TEXT NOT NULL,
    -- 注册时间。
    created_at    TIMESTAMPTZ NOT NULL DEFAULT now(),
    -- 最近一次登录时间。
    last_login_at TIMESTAMPTZ
);
