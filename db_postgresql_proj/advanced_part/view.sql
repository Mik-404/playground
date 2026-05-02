CREATE VIEW cicd.v_active_pipelines AS
SELECT p.pipeline_id, r.name AS repo_name, b.branch_name, d.nickname AS trigger_by, p.created_at
FROM cicd.Pipeline p
JOIN cicd.Branch b ON p.branch_id = b.branch_id
JOIN cicd.Repository r ON b.repository_id = r.repository_id
LEFT JOIN cicd.Developer d ON p.trigger_dev_id = d.developer_id
WHERE p."status" IN ('PENDING', 'RUNNING');

CREATE VIEW cicd.mv_repo_statistics AS
SELECT r.name AS repo_name,
       COUNT(DISTINCT p.pipeline_id) AS total_pipelines,
       SUM(a.size_bytes) / 1024 / 1024 AS total_artifacts_size_mb
FROM cicd.Repository r
LEFT JOIN cicd.Branch b ON r.repository_id = b.repository_id
LEFT JOIN cicd.Pipeline p ON b.branch_id = p.branch_id
LEFT JOIN cicd.Job j ON p.pipeline_id = j.pipeline_id
LEFT JOIN cicd.Artifact a ON j.job_id = a.job_id
GROUP BY r.repository_id, r.name;