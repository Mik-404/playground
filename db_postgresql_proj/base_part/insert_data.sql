-- ==========================================
-- 1. cicd.Developer (22 строки)
-- ==========================================
INSERT INTO cicd.Developer (developer_id, nickname, email, registered_at) VALUES
(1, 'cancel_king', 'king@dev.com', '2023-01-01 10:00:00+00'),
(2, 'cancel_queen', 'queen@dev.com', '2023-01-15 11:30:00+00'),
(3, 'panic_button', 'panic@dev.com', '2023-02-01 09:15:00+00'),
(4, 'alice_wonder', 'alice@dev.com', '2023-03-10 14:00:00+00'),
(5, 'bob_builder', 'bob@dev.com', '2023-04-05 08:45:00+00'),
(6, 'charlie_root', 'charlie@dev.com', '2023-04-20 16:20:00+00'),
(7, 'diana_prince', 'diana@dev.com', '2023-05-12 11:11:00+00'),
(8, 'eve_hacker', 'eve@dev.com', '2023-06-18 19:05:00+00'),
(9, 'frank_castle', 'frank@dev.com', '2023-07-02 07:30:00+00'),
(10, 'grace_hopper', 'grace@dev.com', '2023-08-14 13:25:00+00'),
(11, 'helen_troy', 'helen@dev.com', '2023-09-01 10:50:00+00'),
(12, 'ivan_drago', 'ivan@dev.com', '2023-10-10 15:40:00+00'),
(13, 'jack_sparrow', 'jack@dev.com', '2023-11-20 12:15:00+00'),
(14, 'karen_page', 'karen@dev.com', '2023-12-05 09:00:00+00'),
(15, 'leo_dicap', 'leo@dev.com', '2024-01-15 18:30:00+00'),
(16, 'mia_wallace', 'mia@dev.com', '2024-02-28 14:45:00+00'),
(17, 'neo_matrix', 'neo@dev.com', '2024-03-10 10:10:00+00'),
(18, 'olivia_pope', 'olivia@dev.com', '2024-04-05 08:55:00+00'),
(19, 'peter_parker', 'peter@dev.com', '2024-05-20 16:35:00+00'),
(20, 'quinn_m', 'quinn@dev.com', '2024-06-12 11:20:00+00'),
(21, 'rick_sanchez', 'rick@dev.com', '2024-07-01 09:40:00+00'),
(23, 'gregor_eisenhorn', 'gregor_ei@inc.wh', '4102-03-06 06:06:06+00'),
(22, 'sarah_connor', 'sarah@dev.com', '2024-08-15 13:50:00+00');

-- ==========================================
-- 2. cicd.Repository (20 строк)
-- ==========================================
INSERT INTO cicd.Repository (repository_id, "name", "url") VALUES
(1, 'core-api', 'https://github.com/company/core-api.git'),
(2, 'frontend-web', 'https://github.com/company/frontend-web.git'),
(3, 'auth-service', 'https://gitlab.com/company/auth-service.git'),
(4, 'billing-engine', 'https://github.com/company/billing-engine.git'),
(5, 'notification-srv', 'https://bitbucket.org/company/notification-srv.git'),
(6, 'user-profile', 'https://github.com/company/user-profile.git'),
(7, 'data-warehouse', 'https://gitlab.com/company/data-warehouse.git'),
(8, 'mobile-ios', 'https://github.com/company/mobile-ios.git'),
(9, 'mobile-android', 'https://github.com/company/mobile-android.git'),
(10, 'infra-as-code', 'https://github.com/company/infra-as-code.git'),
(11, 'ml-models', 'https://gitlab.com/company/ml-models.git'),
(12, 'payment-gateway', 'https://github.com/company/payment-gateway.git'),
(13, 'search-service', 'https://github.com/company/search-service.git'),
(14, 'cache-manager', 'https://bitbucket.org/company/cache-manager.git'),
(15, 'log-aggregator', 'https://github.com/company/log-aggregator.git'),
(16, 'analytics-ui', 'https://gitlab.com/company/analytics-ui.git'),
(17, 'cdn-config', 'https://github.com/company/cdn-config.git'),
(18, 'customer-support', 'https://github.com/company/customer-support.git'),
(19, 'admin-panel', 'https://github.com/company/admin-panel.git'),
(20, 'legacy-monolith', 'https://github.com/company/legacy-monolith.git');

-- ==========================================
-- 3. cicd.Repository_Collaborator (25 строк)
-- Пример связи 1-ко-многим и многие-ко-многим
-- ==========================================
INSERT INTO cicd.Repository_Collaborator (developer_id, repository_id, access_role, added_at) VALUES
(1, 1, 'OWNER', '2023-01-02 10:00:00+00'),
(2, 1, 'MAINTAINER', '2023-01-16 10:00:00+00'),
(3, 1, 'DEVELOPER', '2023-02-05 10:00:00+00'),
(4, 2, 'OWNER', '2023-03-11 10:00:00+00'),
(5, 2, 'DEVELOPER', '2023-04-06 10:00:00+00'),
(1, 3, 'OWNER', '2023-05-01 10:00:00+00'),
(6, 4, 'OWNER', '2023-04-21 10:00:00+00'),
(7, 5, 'OWNER', '2023-05-13 10:00:00+00'),
(8, 6, 'OWNER', '2023-06-19 10:00:00+00'),
(9, 7, 'OWNER', '2023-07-03 10:00:00+00'),
(10, 8, 'OWNER', '2023-08-15 10:00:00+00'),
(11, 9, 'OWNER', '2023-09-02 10:00:00+00'),
(12, 10, 'OWNER', '2023-10-11 10:00:00+00'),
(13, 11, 'OWNER', '2023-11-21 10:00:00+00'),
(14, 12, 'OWNER', '2023-12-06 10:00:00+00'),
(15, 13, 'OWNER', '2024-01-16 10:00:00+00'),
(16, 14, 'OWNER', '2024-02-29 10:00:00+00'),
(17, 15, 'OWNER', '2024-03-11 10:00:00+00'),
(18, 16, 'OWNER', '2024-04-06 10:00:00+00'),
(19, 17, 'OWNER', '2024-05-21 10:00:00+00'),
(20, 18, 'OWNER', '2024-06-13 10:00:00+00'),
(21, 19, 'OWNER', '2024-07-02 10:00:00+00'),
(22, 20, 'OWNER', '2024-08-16 10:00:00+00'),
(2, 20, 'READER', '2024-08-17 10:00:00+00'),
(3, 19, 'DEVELOPER', '2024-08-18 10:00:00+00');

-- ==========================================
-- 4. cicd.Branch (23 строки)
-- 1 Репозиторий имеет несколько веток
-- ==========================================
INSERT INTO cicd.Branch (branch_id, repository_id, branch_name, is_protected) VALUES
(1, 1, 'main', TRUE),
(24, 1, 'release', TRUE),
(2, 1, 'develop', FALSE),
(3, 1, 'feature/auth-fix', FALSE),
(4, 2, 'main', TRUE),
(25, 3, 'release', TRUE),
(5, 2, 'hotfix/ui-bug', FALSE),
(6, 3, 'master', TRUE),
(7, 3, 'feature/oauth2', FALSE),
(8, 4, 'main', TRUE),
(9, 5, 'main', TRUE),
(10, 6, 'develop', FALSE),
(11, 7, 'main', TRUE),
(12, 8, 'release/v1.0', TRUE),
(13, 9, 'main', TRUE),
(14, 10, 'main', TRUE),
(15, 11, 'experiment/model-v2', FALSE),
(16, 12, 'main', TRUE),
(17, 13, 'develop', FALSE),
(18, 14, 'main', TRUE),
(19, 15, 'feature/elk-stack', FALSE),
(20, 16, 'main', TRUE),
(21, 17, 'master', TRUE),
(22, 18, 'main', TRUE),
(26, 20, 'dev', FALSE),
(23, 19, 'main', TRUE);

-- ==========================================
-- 5. cicd.Pipeline (26 строк)
-- Разработчики 1, 2, 3 имеют больше CANCELLED (3-4 шт), чем SUCCESS (0-1 шт)
-- ==========================================
INSERT INTO cicd.Pipeline (pipeline_id, branch_id, trigger_dev_id, commit_hash, "status", created_at, started_at, finished_at) VALUES
-- Dev 1: 4 Cancelled, 1 Success
(1, 1, 1, 'a1b2c3d4e5f6g7h8i9j0', 'CANCELLED', '2024-01-01 10:00:00+00', '2024-01-01 10:01:00+00', '2024-01-01 10:02:00+00'),
(2, 2, 1, 'b2c3d4e5f6g7h8i9j0a1', 'CANCELLED', '2024-01-02 11:00:00+00', '2024-01-02 11:01:00+00', '2024-01-02 11:02:00+00'),
(3, 3, 1, 'c3d4e5f6g7h8i9j0a1b2', 'CANCELLED', '2024-01-03 12:00:00+00', '2024-01-03 12:01:00+00', '2024-01-03 12:05:00+00'),
(4, 1, 1, 'd4e5f6g7h8i9j0a1b2c3', 'CANCELLED', '2024-01-04 13:00:00+00', '2024-01-04 13:01:00+00', '2024-01-04 13:02:00+00'),
(5, 2, 1, 'e5f6g7h8i9j0a1b2c3d4', 'SUCCESS',   '2024-01-05 14:00:00+00', '2024-01-05 14:01:00+00', '2024-01-05 14:10:00+00'),

-- Dev 2: 3 Cancelled, 0 Success
(6, 4, 2, 'f6g7h8i9j0a1b2c3d4e5', 'CANCELLED', '2024-02-01 09:00:00+00', '2024-02-01 09:01:00+00', '2024-02-01 09:03:00+00'),
(7, 5, 2, 'g7h8i9j0a1b2c3d4e5f6', 'CANCELLED', '2024-02-02 10:00:00+00', '2024-02-02 10:01:00+00', '2024-02-02 10:04:00+00'),
(8, 4, 2, 'h8i9j0a1b2c3d4e5f6g7', 'CANCELLED', '2024-02-03 11:00:00+00', '2024-02-03 11:01:00+00', '2024-02-03 11:02:00+00'),

-- Dev 3: 3 Cancelled, 1 Success
(9,  6, 3, 'i9j0a1b2c3d4e5f6g7h8', 'CANCELLED', '2024-03-01 08:00:00+00', '2024-03-01 08:01:00+00', '2024-03-01 08:05:00+00'),
(10, 7, 3, 'j0a1b2c3d4e5f6g7h8i9', 'CANCELLED', '2024-03-02 09:00:00+00', '2024-03-02 09:01:00+00', '2024-03-02 09:06:00+00'),
(11, 6, 3, 'k1l2m3n4o5p6q7r8s9t0', 'CANCELLED', '2024-03-03 10:00:00+00', '2024-03-03 10:01:00+00', '2024-03-03 10:02:00+00'),
(12, 7, 3, 'l2m3n4o5p6q7r8s9t0k1', 'SUCCESS',   '2024-03-04 11:00:00+00', '2024-03-04 11:01:00+00', '2024-03-04 11:15:00+00'),

-- Other Devs (Various Statuses)
(13, 8,  4, 'm3n4o5p6q7r8s9t0k1l2', 'SUCCESS', '2024-04-01 10:00:00+00', '2024-04-01 10:01:00+00', '2024-04-01 10:10:00+00'),
(14, 9,  NULL, 'n4o5p6q7r8s9t0k1l2m3', 'FAILED',  '2024-04-02 11:00:00+00', '2024-04-02 11:01:00+00', '2024-04-02 11:05:00+00'),
(15, 10, 6, 'o5p6q7r8s9t0k1l2m3n4', 'SUCCESS', '2024-04-03 12:00:00+00', '2024-04-03 12:01:00+00', '2024-04-03 12:12:00+00'),
(16, 11, 7, 'p6q7r8s9t0k1l2m3n4o5', 'PENDING', '2024-04-04 13:00:00+00', NULL, NULL),
(17, 12, 8, 'q7r8s9t0k1l2m3n4o5p6', 'RUNNING', '2024-04-05 14:00:00+00', '2024-04-05 14:01:00+00', NULL),
(18, 13, 9, 'r8s9t0k1l2m3n4o5p6q7', 'SUCCESS', '2024-04-06 15:00:00+00', '2024-04-06 15:01:00+00', '2024-04-06 15:08:00+00'),
(19, 14, 10,'s9t0k1l2m3n4o5p6q7r8', 'FAILED',  '2024-04-07 16:00:00+00', '2024-04-07 16:01:00+00', '2024-04-07 16:05:00+00'),
(20, 15, NULL,'t0k1l2m3n4o5p6q7r8s9', 'SUCCESS', '2024-04-08 17:00:00+00', '2024-04-08 17:01:00+00', '2024-04-08 17:20:00+00'),
(21, 16, 12,'u1v2w3x4y5z6a7b8c9d0', 'SUCCESS', '2024-04-09 18:00:00+00', '2024-04-09 18:01:00+00', '2024-04-09 18:11:00+00'),
(22, 17, 13,'v2w3x4y5z6a7b8c9d0u1', 'RUNNING',  '2024-04-10 19:00:00+00', '2024-04-10 19:01:00+00', '2024-04-10 19:07:00+00'),
(23, 18, 14,'w3x4y5z6a7b8c9d0u1v2', 'SUCCESS', '2024-04-11 20:00:00+00', '2024-04-11 20:01:00+00', '2024-04-11 20:15:00+00'),
(24, 19, 15,'x4y5z6a7b8c9d0u1v2w3', 'SUCCESS', '2024-04-12 21:00:00+00', '2024-04-12 21:01:00+00', '2024-04-12 21:10:00+00'),
(25, 20, 16,'y5z6a7b8c9d0u1v2w3x4', 'RUNNING', '2024-04-13 22:00:00+00', '2024-04-13 22:01:00+00', NULL),
(26, 21, 17,'z6a7b8c9d0u1v2w3x4y5', 'SUCCESS', '2024-04-14 23:00:00+00', '2024-04-14 23:01:00+00', '2024-04-14 23:18:00+00');

-- ==========================================
-- 6. cicd.Runner (20 строк)
-- ==========================================
INSERT INTO cicd.Runner (runner_sk, runner_uuid, os_type, ram_mb, "status", valid_from, valid_to, is_current) VALUES
(1,  '11111111-1111-1111-1111-111111111111', 'Linux', 4096, 'IDLE', '2023-01-01 00:00:00+00', '9999-12-31 23:59:59+00', TRUE),
(2,  '22222222-2222-2222-2222-222222222222', 'Linux', 8192, 'BUSY', '2023-01-01 00:00:00+00', '9999-12-31 23:59:59+00', TRUE),
(3,  '33333333-3333-3333-3333-333333333333', 'Windows', 8192, 'IDLE', '2023-01-01 00:00:00+00', '9999-12-31 23:59:59+00', TRUE),
(4,  '44444444-4444-4444-4444-444444444444', 'macOS', 16384, 'IDLE', '2023-01-01 00:00:00+00', '9999-12-31 23:59:59+00', TRUE),
(5,  '55555555-5555-5555-5555-555555555555', 'Linux', 2048, 'OFFLINE', '2023-01-01 00:00:00+00', '2023-12-31 23:59:59+00', FALSE),
(6,  '66666666-6666-6666-6666-666666666666', 'Linux', 4096, 'BUSY', '2023-01-01 00:00:00+00', '9999-12-31 23:59:59+00', TRUE),
(7,  '77777777-7777-7777-7777-777777777777', 'Windows', 4096, 'IDLE', '2023-01-01 00:00:00+00', '9999-12-31 23:59:59+00', TRUE),
(8,  '88888888-8888-8888-8888-888888888888', 'Linux', 16384, 'IDLE', '2023-01-01 00:00:00+00', '9999-12-31 23:59:59+00', TRUE),
(9,  '99999999-9999-9999-9999-999999999999', 'macOS', 8192, 'IDLE', '2023-01-01 00:00:00+00', '9999-12-31 23:59:59+00', TRUE),
(10, 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa', 'Linux', 32768, 'IDLE', '2023-01-01 00:00:00+00', '9999-12-31 23:59:59+00', TRUE),
(11, 'bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb', 'Linux', 4096, 'IDLE', '2023-01-01 00:00:00+00', '9999-12-31 23:59:59+00', TRUE),
(12, 'cccccccc-cccc-cccc-cccc-cccccccccccc', 'Windows', 16384, 'OFFLINE', '2023-01-01 00:00:00+00', '2024-01-01 00:00:00+00', FALSE),
(13, 'dddddddd-dddd-dddd-dddd-dddddddddddd', 'Linux', 8192, 'IDLE', '2023-01-01 00:00:00+00', '9999-12-31 23:59:59+00', TRUE),
(14, 'eeeeeeee-eeee-eeee-eeee-eeeeeeeeeeee', 'macOS', 8192, 'BUSY', '2023-01-01 00:00:00+00', '9999-12-31 23:59:59+00', TRUE),
(15, 'ffffffff-ffff-ffff-ffff-ffffffffffff', 'Linux', 4096, 'IDLE', '2023-01-01 00:00:00+00', '9999-12-31 23:59:59+00', TRUE),
(16, '00000000-0000-0000-0000-000000000001', 'Windows', 8192, 'IDLE', '2023-01-01 00:00:00+00', '9999-12-31 23:59:59+00', TRUE),
(17, '00000000-0000-0000-0000-000000000002', 'Linux', 16384, 'IDLE', '2023-01-01 00:00:00+00', '9999-12-31 23:59:59+00', TRUE),
(18, '00000000-0000-0000-0000-000000000003', 'macOS', 32768, 'IDLE', '2023-01-01 00:00:00+00', '9999-12-31 23:59:59+00', TRUE),
(19, '00000000-0000-0000-0000-000000000004', 'Linux', 4096, 'OFFLINE', '2023-01-01 00:00:00+00', '9999-12-31 23:59:59+00', TRUE),
(20, '00000000-0000-0000-0000-000000000005', 'Windows', 4096, 'IDLE', '2023-01-01 00:00:00+00', '9999-12-31 23:59:59+00', TRUE);

-- ==========================================
-- 7. cicd.Job (25 строк)
-- Пример связи: у Pipeline 5 есть 3 джобы (build, test, deploy)
-- ==========================================
INSERT INTO cicd.Job (job_id, pipeline_id, runner_sk, job_name, required_os, "status", started_at, finished_at) VALUES
(1, 1, 1, 'build-linux', 'Linux', 'CANCELLED', '2024-01-01 10:01:00+00', '2024-01-01 10:02:00+00'),
(2, 2, 2, 'test-linux', 'Linux', 'CANCELLED', '2024-01-02 11:01:00+00', '2024-01-02 11:02:00+00'),
(3, 3, 3, 'build-windows', 'Windows', 'CANCELLED', '2024-01-03 12:01:00+00', '2024-01-03 12:05:00+00'),
(4, 4, 1, 'deploy-linux', 'Linux', 'CANCELLED', '2024-01-04 13:01:00+00', '2024-01-04 13:02:00+00'),
-- Pipeline 5 (SUCCESS) с 3 джобами
(5, 5, 1, 'build', 'Linux', 'SUCCESS', '2024-01-05 14:01:00+00', '2024-01-05 14:03:00+00'),
(6, 5, 2, 'test', 'Linux', 'SUCCESS', '2024-01-05 14:04:00+00', '2024-01-05 14:08:00+00'),
(7, 5, 1, 'deploy', 'Linux', 'SUCCESS', '2024-01-05 14:08:30+00', '2024-01-05 14:10:00+00'),

(8, 6, 4, 'build-macos', 'macOS', 'CANCELLED', '2024-02-01 09:01:00+00', '2024-02-01 09:03:00+00'),
(9, 7, 1, 'linting', 'Linux', 'CANCELLED', '2024-02-02 10:01:00+00', '2024-02-02 10:04:00+00'),
(10, 8, 3, 'test-windows', 'Windows', 'CANCELLED', '2024-02-03 11:01:00+00', '2024-02-03 11:02:00+00'),
(11, 9, 2, 'build-backend', 'Linux', 'CANCELLED', '2024-03-01 08:01:00+00', '2024-03-01 08:05:00+00'),
(12, 10, 1, 'build-frontend', 'Linux', 'CANCELLED', '2024-03-02 09:01:00+00', '2024-03-02 09:06:00+00'),
(13, 11, 4, 'ios-build', 'macOS', 'CANCELLED', '2024-03-03 10:01:00+00', '2024-03-03 10:02:00+00'),
(14, 12, 1, 'compile', 'Linux', 'SUCCESS', '2024-03-04 11:01:00+00', '2024-03-04 11:15:00+00'),
(15, 13, 3, 'win-exe-build', 'Windows', 'SUCCESS', '2024-04-01 10:01:00+00', '2024-04-01 10:10:00+00'),
(16, 14, 1, 'docker-build', 'Linux', 'FAILED', '2024-04-02 11:01:00+00', '2024-04-02 11:05:00+00'),
(17, 15, 2, 'e2e-tests', 'Linux', 'SUCCESS', '2024-04-03 12:01:00+00', '2024-04-03 12:12:00+00'),
(18, 16, NULL, 'pending-job', 'Linux', 'PENDING', NULL, NULL),
(19, 17, 2, 'running-job', 'Linux', 'RUNNING', '2024-04-05 14:01:00+00', NULL),
(20, 18, 4, 'mac-tests', 'macOS', 'SUCCESS', '2024-04-06 15:01:00+00', '2024-04-06 15:08:00+00'),
(21, 19, 1, 'k8s-deploy', 'Linux', 'FAILED', '2024-04-07 16:01:00+00', '2024-04-07 16:05:00+00'),
(22, 20, 3, 'win-test', 'Windows', 'SUCCESS', '2024-04-08 17:01:00+00', '2024-04-08 17:20:00+00'),
(23, 21, 1, 'sonar-scan', 'Linux', 'SUCCESS', '2024-04-09 18:01:00+00', '2024-04-09 18:11:00+00'),
(24, 22, 2, 'db-migration', 'Linux', 'RUNNING', '2024-04-10 19:01:00+00', '2024-04-10 19:07:00+00'),
(26, 24, 2, 'build-parser', 'Linux', 'FAILED', '2024-04-10 19:01:00+00', '2024-04-10 19:07:00+00'),
(27, 25, 2, 'db-migration', 'Linux', 'RUNNING', '2024-04-10 19:01:00+00', '2024-04-10 19:07:00+00'),
(25, 23, 1, 'cache-warmup', 'Linux', 'SUCCESS', '2024-04-11 20:01:00+00', '2024-04-11 20:15:00+00');

-- ==========================================
-- 8. cicd.Artifact (22 строки)
-- Пример связи: у Job 5 есть 2 артефакта
-- ==========================================
INSERT INTO cicd.Artifact (artifact_id, job_id, file_name, file_path, size_bytes, uploaded_at) VALUES
(1, 5, 'build.zip', '/artifacts/pipeline_5/build.zip', 10485760, '2024-01-05 14:03:00+00'),
(2, 5, 'meta.json', '/artifacts/pipeline_5/meta.json', 2048, '2024-01-05 14:03:05+00'),
(3, 6, 'test-report.xml', '/artifacts/pipeline_5/test-report.xml', 512000, '2024-01-05 14:08:00+00'),
(4, 7, 'deploy-log.txt', '/artifacts/pipeline_5/deploy-log.txt', 15000, '2024-01-05 14:10:00+00'),
(5, 14, 'binary.bin', '/artifacts/pipeline_12/binary.bin', 45000000, '2024-03-04 11:15:00+00'),
(6, 15, 'app.exe', '/artifacts/pipeline_13/app.exe', 15000000, '2024-04-01 10:10:00+00'),
(7, 16, 'error-log.txt', '/artifacts/pipeline_14/error-log.txt', 3000, '2024-04-02 11:05:00+00'),
(8, 17, 'e2e-results.html', '/artifacts/pipeline_15/e2e-results.html', 120000, '2024-04-03 12:12:00+00'),
(9, 20, 'ios-app.ipa', '/artifacts/pipeline_18/ios-app.ipa', 85000000, '2024-04-06 15:08:00+00'),
(10, 21, 'k8s-crash.log', '/artifacts/pipeline_19/k8s-crash.log', 25600, '2024-04-07 16:05:00+00'),
(11, 22, 'coverage.xml', '/artifacts/pipeline_20/coverage.xml', 45000, '2024-04-08 17:20:00+00'),
(12, 23, 'sonar-report.pdf', '/artifacts/pipeline_21/sonar-report.pdf', 3500000, '2024-04-09 18:11:00+00'),
(13, 24, 'db-dump.sql', '/artifacts/pipeline_22/db-dump.sql', 1500000, '2024-04-10 19:07:00+00'),
(14, 25, 'cache.tar.gz', '/artifacts/pipeline_23/cache.tar.gz', 25000000, '2024-04-11 20:15:00+00'),
(15, 1, 'partial_build.log', '/artifacts/pipeline_1/partial_build.log', 1024, '2024-01-01 10:02:00+00'),
(16, 2, 'partial_test.log', '/artifacts/pipeline_2/partial_test.log', 2048, '2024-01-02 11:02:00+00'),
(17, 3, 'win_partial.log', '/artifacts/pipeline_3/win_partial.log', 512, '2024-01-03 12:05:00+00'),
(18, 4, 'deploy_fail.log', '/artifacts/pipeline_4/deploy_fail.log', 4096, '2024-01-04 13:02:00+00'),
(19, 8, 'mac_cancel.log', '/artifacts/pipeline_6/mac_cancel.log', 1024, '2024-02-01 09:03:00+00'),
(20, 9, 'lint_cancel.log', '/artifacts/pipeline_7/lint_cancel.log', 2048, '2024-02-02 10:04:00+00'),
(21, 10, 'win_test_err.log', '/artifacts/pipeline_8/win_test_err.log', 1024, '2024-02-03 11:02:00+00'),
(22, 11, 'backend_err.log', '/artifacts/pipeline_9/backend_err.log', 4096, '2024-03-01 08:05:00+00');

SELECT setval('cicd.developer_developer_id_seq', (SELECT MAX(developer_id) FROM cicd.Developer));
SELECT setval('cicd.repository_repository_id_seq', (SELECT MAX(repository_id) FROM cicd.Repository));
SELECT setval('cicd.branch_branch_id_seq', (SELECT MAX(branch_id) FROM cicd.Branch));
SELECT setval('cicd.pipeline_pipeline_id_seq', (SELECT MAX(pipeline_id) FROM cicd.Pipeline));
SELECT setval('cicd.runner_runner_sk_seq', (SELECT MAX(runner_sk) FROM cicd.Runner));
SELECT setval('cicd.job_job_id_seq', (SELECT MAX(job_id) FROM cicd.Job));
SELECT setval('cicd.artifact_artifact_id_seq', (SELECT MAX(artifact_id) FROM cicd.Artifact));
