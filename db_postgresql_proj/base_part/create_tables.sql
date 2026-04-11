CREATE SCHEMA IF NOT EXISTS cicd;

CREATE TABLE cicd.Developer (
    developer_id BIGSERIAL,
    nickname VARCHAR(50) NOT NULL,
    email VARCHAR(255) NOT NULL,
    registered_at TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP NOT NULL,

    CONSTRAINT pk_developer PRIMARY KEY (developer_id),
    CONSTRAINT uq_developer_nickname UNIQUE (nickname),
    CONSTRAINT uq_developer_email UNIQUE (email),
    CONSTRAINT chk_developer_email_format CHECK (email ~* '^[A-Z0-9._%+-]+@[A-Z0-9.-]+\.[A-Z]{2,4}$')
);

CREATE TABLE cicd.Repository (
    repository_id BIGSERIAL,
    "name" VARCHAR(100) NOT NULL,
    "url" VARCHAR(1000) NOT NULL,

    CONSTRAINT pk_repository PRIMARY KEY (repository_id),
    CONSTRAINT uq_repository_url UNIQUE ("url"),
    CONSTRAINT chk_repository_url_format CHECK ("url" ~* '^(https?://|ssh://)?([a-z0-9_.-]+@)?[a-z0-9.-]+([:/][a-z0-9./~_?&=+-]+)?$')
);

CREATE TABLE cicd.Repository_Collaborator (
    developer_id BIGINT NOT NULL,
    repository_id BIGINT NOT NULL,
    access_role VARCHAR(20) DEFAULT 'READER' NOT NULL,
    added_at TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP NOT NULL,
    
    CONSTRAINT pk_repository_collaborator PRIMARY KEY (developer_id, repository_id),
    CONSTRAINT fk_repo_collab_developer FOREIGN KEY (developer_id) REFERENCES cicd.Developer(developer_id) ON DELETE CASCADE,
    CONSTRAINT fk_repo_collab_repository FOREIGN KEY (repository_id) REFERENCES cicd.Repository(repository_id) ON DELETE CASCADE,
    CONSTRAINT chk_collaborator_access_role CHECK (access_role IN ('OWNER', 'MAINTAINER', 'DEVELOPER', 'READER'))
);

CREATE TABLE cicd.Branch (
    branch_id BIGSERIAL,
    repository_id BIGINT NOT NULL,
    branch_name VARCHAR(100) NOT NULL,
    is_protected BOOLEAN DEFAULT FALSE NOT NULL,

    CONSTRAINT pk_branch PRIMARY KEY (branch_id),
    CONSTRAINT fk_branch_repository FOREIGN KEY (repository_id) REFERENCES cicd.Repository(repository_id) ON DELETE CASCADE,
    CONSTRAINT uq_branch_repo_name UNIQUE (repository_id, branch_name)
);

CREATE TABLE cicd.Pipeline (
    pipeline_id BIGSERIAL,
    branch_id BIGINT NOT NULL,
    trigger_dev_id BIGINT,
    commit_hash VARCHAR(128) NOT NULL,
    "status" VARCHAR(20) DEFAULT 'PENDING' NOT NULL,
    created_at TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP NOT NULL,
    started_at TIMESTAMPTZ,
    finished_at TIMESTAMPTZ,

    CONSTRAINT pk_pipeline PRIMARY KEY (pipeline_id),
    CONSTRAINT fk_pipeline_branch FOREIGN KEY (branch_id) REFERENCES cicd.Branch(branch_id) ON DELETE CASCADE,
    CONSTRAINT fk_pipeline_developer FOREIGN KEY (trigger_dev_id) REFERENCES cicd.Developer(developer_id) ON DELETE SET NULL,
    CONSTRAINT chk_pipeline_status CHECK ("status" IN ('PENDING', 'RUNNING', 'SUCCESS', 'FAILED', 'CANCELLED')),
    CONSTRAINT chk_pipeline_timestamps CHECK (
        (started_at IS NULL OR started_at >= created_at) AND
        (finished_at IS NULL OR started_at IS NULL OR finished_at >= started_at)
    )
);

CREATE TABLE cicd.Runner (
    runner_sk BIGSERIAL,
    runner_uuid UUID DEFAULT gen_random_uuid() NOT NULL,
    os_type VARCHAR(50) NOT NULL,
    ram_mb INT NOT NULL,
    "status" VARCHAR(20) DEFAULT 'IDLE' NOT NULL,
    valid_from TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP NOT NULL,
    valid_to TIMESTAMPTZ DEFAULT '9999-12-31 23:59:59' NOT NULL,
    is_current BOOLEAN DEFAULT TRUE NOT NULL,

    CONSTRAINT pk_runner PRIMARY KEY (runner_sk),
    CONSTRAINT chk_runner_ram CHECK (ram_mb > 0),
    CONSTRAINT chk_runner_status CHECK ("status" IN ('IDLE', 'BUSY', 'OFFLINE')),
    CONSTRAINT chk_runner_valid_dates CHECK (valid_to >= valid_from)
);

CREATE TABLE cicd.Job (
    job_id BIGSERIAL,
    pipeline_id BIGINT NOT NULL,
    runner_sk BIGINT,
    job_name VARCHAR(100) NOT NULL,
    required_os VARCHAR(50) NOT NULL,
    "status" VARCHAR(20) DEFAULT 'PENDING' NOT NULL,
    started_at TIMESTAMPTZ,
    finished_at TIMESTAMPTZ,

    CONSTRAINT pk_job PRIMARY KEY (job_id),
    CONSTRAINT fk_job_pipeline FOREIGN KEY (pipeline_id) REFERENCES cicd.Pipeline(pipeline_id) ON DELETE CASCADE,
    CONSTRAINT fk_job_runner FOREIGN KEY (runner_sk) REFERENCES cicd.Runner(runner_sk) ON DELETE SET NULL,
    CONSTRAINT chk_job_status CHECK ("status" IN ('PENDING', 'RUNNING', 'SUCCESS', 'FAILED', 'CANCELLED')),
    CONSTRAINT chk_job_timestamps CHECK (finished_at IS NULL OR started_at IS NULL OR finished_at >= started_at)
);

CREATE TABLE cicd.Artifact (
    artifact_id BIGSERIAL,
    job_id BIGINT NOT NULL,
    file_name VARCHAR(255) NOT NULL,
    file_path VARCHAR(512) NOT NULL,
    size_bytes BIGINT NOT NULL,
    uploaded_at TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP NOT NULL,
    
    CONSTRAINT pk_artifact PRIMARY KEY (artifact_id),
    CONSTRAINT fk_artifact_job FOREIGN KEY (job_id) REFERENCES cicd.Job(job_id) ON DELETE CASCADE,
    CONSTRAINT chk_artifact_size CHECK (size_bytes >= 0),
    CONSTRAINT chk_artifact_filename_format CHECK (file_name ~ '^[^\s/\\][^\/\\]{0,254}$'),
    CONSTRAINT chk_artifact_filepath_format CHECK (file_path ~ '^(/[^/]+)+$')
);