-- Получить полную информацию о выполняющихся задачах
SELECT j.job_name, p.status AS pipe_status, b.branch_name, r.name AS repo_name, d.email AS dev_email
FROM cicd.Job j
JOIN cicd.Pipeline p ON j.pipeline_id = p.pipeline_id
JOIN cicd.Branch b ON p.branch_id = b.branch_id
JOIN cicd.Repository r ON b.repository_id = r.repository_id
LEFT JOIN cicd.Developer d ON p.trigger_dev_id = d.developer_id
WHERE j.status = 'RUNNING';

-- Репозитории, в которых больше 1 защищенной ветки
SELECT r.name, COUNT(b.branch_id) AS protected_branches_count
FROM cicd.Repository r
JOIN cicd.Branch b ON r.repository_id = b.repository_id
WHERE b.is_protected = TRUE
GROUP BY r.name
HAVING COUNT(b.branch_id) > 1;

-- Найти раннеры, которые ни разу не выполняли Job со статусом FAILED
SELECT runner_uuid, os_type, ram_mb 
FROM cicd.Runner 
WHERE runner_sk NOT IN (
    SELECT DISTINCT runner_sk FROM cicd.Job WHERE status = 'FAILED' AND runner_sk IS NOT NULL
);

-- Топ разработчиков по количеству запущенных пайплайнов в каждом проекте
WITH DevStats AS (
    SELECT r.name AS repo, d.nickname, COUNT(p.pipeline_id) AS run_count
    FROM cicd.Pipeline p
    JOIN cicd.Branch b ON p.branch_id = b.branch_id
    JOIN cicd.Repository r ON b.repository_id = r.repository_id
    JOIN cicd.Developer d ON p.trigger_dev_id = d.developer_id
    GROUP BY r.name, d.nickname
)
SELECT d1.repo, d1.nickname, d1.run_count
FROM DevStats d1
JOIN (
    SELECT repo, MAX(run_count) as max_runs
    FROM DevStats
    GROUP BY repo
) d2 ON d1.repo = d2.repo AND d1.run_count = d2.max_runs
ORDER BY d1.run_count DESC;

-- Среднее время работы Pipeline в ветке в секундах, только для успешных
SELECT b.branch_name, 
       AVG(EXTRACT(EPOCH FROM (p.finished_at - p.started_at))) AS avg_duration_sec
FROM cicd.Pipeline p
JOIN cicd.Branch b ON p.branch_id = b.branch_id
WHERE p.status = 'SUCCESS' AND p.finished_at IS NOT NULL
GROUP BY b.branch_name;

-- Поиск веток, в которых ни разу не запускался Pipeline
SELECT r.name AS repo_name, b.branch_name 
FROM cicd.Branch b
LEFT JOIN cicd.Pipeline p ON b.branch_id = p.branch_id
JOIN cicd.Repository r ON b.repository_id = r.repository_id
WHERE p.pipeline_id IS NULL;

-- Сводная статистика статусов Job по ОС раннеров
SELECT required_os,
       COUNT(*) AS total_jobs,
       COUNT(CASE WHEN status = 'SUCCESS' THEN 1 END) AS success_jobs,
       COUNT(CASE WHEN status = 'FAILED' THEN 1 END) AS failed_jobs
FROM cicd.Job
GROUP BY required_os;

-- Подсчет суммарного, максимального размера и количества артефактов для каждой задачи
SELECT j.job_id, j.job_name,
       COUNT(a.artifact_id) AS total_files,
       SUM(a.size_bytes) AS total_size_bytes,
       MAX(a.size_bytes) AS max_file_size
FROM cicd.Artifact a
JOIN cicd.Job j ON a.job_id = j.job_id
GROUP BY j.job_id
ORDER BY total_size_bytes DESC;

-- Репозитории, занимающие больше всего места (сумма всех артефактов по всем пайплайнам)
WITH RepoArtifacts AS (
    SELECT r.name, a.size_bytes
    FROM cicd.Artifact a
    JOIN cicd.Job j ON a.job_id = j.job_id
    JOIN cicd.Pipeline p ON j.pipeline_id = p.pipeline_id
    JOIN cicd.Branch b ON p.branch_id = b.branch_id
    JOIN cicd.Repository r ON b.repository_id = r.repository_id
)
SELECT "name", SUM(size_bytes) / 1024 / 1024 AS total_size_mb
FROM RepoArtifacts
GROUP BY "name"
ORDER BY total_size_mb DESC;

-- Найти разработчиков, которые отменили свои Pipeline чаще, чем они завершались успешно
SELECT d.nickname,
       COUNT(CASE WHEN p.status = 'CANCELLED' THEN 1 END) AS canceled_count,
       COUNT(CASE WHEN p.status = 'SUCCESS' THEN 1 END) AS success_count
FROM cicd.Developer d
JOIN cicd.Pipeline p ON d.developer_id = p.trigger_dev_id
GROUP BY d.nickname
HAVING COUNT(CASE WHEN p.status = 'CANCELLED' THEN 1 END) > COUNT(CASE WHEN p.status = 'SUCCESS' THEN 1 END);