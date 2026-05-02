CREATE INDEX idx_job_pipeline_id ON cicd.Job(pipeline_id);

CREATE INDEX idx_pipeline_active_status 
ON cicd.Pipeline("status") 
WHERE "status" IN ('PENDING', 'RUNNING');

CREATE INDEX idx_runner_current 
ON cicd.Runner(os_type) 
WHERE is_current = TRUE;