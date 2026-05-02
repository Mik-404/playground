CREATE OR REPLACE PROCEDURE cicd.update_runner_config(
    p_uuid UUID, p_new_os VARCHAR, p_new_ram INT
) LANGUAGE plpgsql AS $$
BEGIN
    UPDATE cicd.Runner
    SET valid_to = CURRENT_TIMESTAMP, is_current = FALSE
    WHERE runner_uuid = p_uuid AND is_current = TRUE;

    INSERT INTO cicd.Runner (runner_uuid, os_type, ram_mb, "status", valid_from, valid_to, is_current)
    VALUES (p_uuid, p_new_os, p_new_ram, 'IDLE', CURRENT_TIMESTAMP, '9999-12-31 23:59:59', TRUE);
END;
$$;

CREATE OR REPLACE PROCEDURE cicd.complete_job(
    p_job_id BIGINT,
    p_final_status VARCHAR
) LANGUAGE plpgsql AS $$
DECLARE
    v_runner_sk BIGINT;
BEGIN
    IF p_final_status NOT IN ('SUCCESS', 'FAILED', 'CANCELLED') THEN
        RAISE EXCEPTION 'Недопустимый финальный статус %', p_final_status;
    END IF;

    SELECT runner_sk INTO v_runner_sk
    FROM cicd.Job
    WHERE job_id = p_job_id AND "status" = 'RUNNING';

    IF NOT FOUND THEN
        RAISE NOTICE 'Задача % уже завершена или еще не запущена.', p_job_id;
        RETURN;
    END IF;

    UPDATE cicd.Job
    SET "status" = p_final_status,
        finished_at = CURRENT_TIMESTAMP
    WHERE job_id = p_job_id;

    IF v_runner_sk IS NOT NULL THEN
        UPDATE cicd.Runner
        SET "status" = 'IDLE'
        WHERE runner_sk = v_runner_sk;
    END IF;
    
    RAISE NOTICE 'Задача % успешно завершена со статусом %', p_job_id, p_final_status;
END;
$$;

CREATE OR REPLACE FUNCTION cicd.cancel_stale_jobs(timeout_minutes INT) 
RETURNS INT LANGUAGE plpgsql AS $$
DECLARE 
    cancelled_count INT;
BEGIN
    WITH updated AS (
        UPDATE cicd.Job 
        SET "status" = 'CANCELLED', finished_at = CURRENT_TIMESTAMP
        WHERE "status" = 'RUNNING' 
          AND started_at < (CURRENT_TIMESTAMP - (timeout_minutes || ' minutes')::interval)
        RETURNING job_id
    )
    SELECT COUNT(*) INTO cancelled_count FROM updated;
    RETURN cancelled_count;
END;
$$;