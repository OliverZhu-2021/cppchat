-- Initialisation script for docker-entrypoint-initdb.d
-- Runs once on first container start when the data volume is empty.
-- Schema reconstructed from SQL strings in source code.

CREATE DATABASE IF NOT EXISTS `ChatHttpServer`
  CHARACTER SET utf8mb4
  COLLATE utf8mb4_unicode_ci;

USE `ChatHttpServer`;

-- users table
-- INSERT INTO users (username, password) — ChatRegisterHandler.cpp
-- SELECT id FROM users WHERE username = ? AND password = ? — ChatLoginHandler.cpp
CREATE TABLE IF NOT EXISTS `users` (
    `id`       INT          NOT NULL AUTO_INCREMENT,
    `username` VARCHAR(255) NOT NULL,
    `password` VARCHAR(255) NOT NULL,
    PRIMARY KEY (`id`),
    UNIQUE KEY `uq_username` (`username`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- chat_message table
-- INSERT INTO chat_message (id, username, session_id, is_user, content, ts) — AIHelper.cpp
-- SELECT ... FROM chat_message ORDER BY ts ASC, id ASC — ChatServer.cpp
-- session_id is BIGINT: AISessionIdGenerator::generate() returns std::to_string(long long)
CREATE TABLE IF NOT EXISTS `chat_message` (
    `id`         BIGINT        NOT NULL COMMENT 'user_id from users.id',
    `username`   VARCHAR(255)  NOT NULL,
    `session_id` BIGINT        NOT NULL,
    `is_user`    TINYINT(1)    NOT NULL DEFAULT 1,
    `content`    LONGTEXT      NOT NULL,
    `ts`         BIGINT        NOT NULL COMMENT 'Unix timestamp in milliseconds',
    KEY `idx_user_session` (`id`, `session_id`),
    KEY `idx_ts`           (`ts`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
