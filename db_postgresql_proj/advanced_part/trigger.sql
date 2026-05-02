CREATE OR REPLACE FUNCTION cicd.check_runner_os_func() RETURNS TRIGGER AS $$
DECLARE 
    v_runner_os VARCHAR;
BEGIN
    IF NEW.runner_sk IS NOT NULL THEN
        SELECT os_type INTO v_runner_os FROM cicd.Runner WHERE runner_sk = NEW.runner_sk;
        
        IF v_runner_os != NEW.required_os THEN
            RAISE EXCEPTION 'Критическая ошибка: Задача % требует %, но назначен Runner с ОС %', NEW.job_id, NEW.required_os, v_runner_os;
        END IF;
    END IF;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_check_runner_os
BEFORE INSERT OR UPDATE OF runner_sk ON cicd.Job
FOR EACH ROW EXECUTE FUNCTION cicd.check_runner_os_func();


CREATE OR REPLACE FUNCTION cicd.prevent_owner_change_func() 
RETURNS TRIGGER AS $$
DECLARE
    v_owner_count INT;
BEGIN
    IF (TG_OP = 'DELETE') THEN
        IF OLD.access_role = 'OWNER' THEN
            RAISE EXCEPTION 'Запрещено удаление последнего владельца репозитория.';
        END IF;
        RETURN OLD;
    END IF;

    IF (TG_OP = 'UPDATE') THEN
        IF OLD.access_role = 'OWNER' AND NEW.access_role != 'OWNER' THEN
            RAISE EXCEPTION 'Запрещено изменять роль последнего владельца репозитория.';
        END IF;
        RETURN NEW;
    END IF;

    IF (TG_OP = 'INSERT') THEN
        IF OLD.access_role = 'OWNER' THEN
            SELECT COUNT(*) INTO v_owner_count
            FROM cicd.Repository_Collaborator
            WHERE repository_id = OLD.repository_id AND access_role = 'OWNER';
            
            IF v_owner_count >= 1 THEN
                RAISE EXCEPTION 'Запрещено создавать больше одного владельца репозитория.';
            END IF;
        END IF;
        RETURN NEW;

    END IF;
    RETURN NEW; 
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_prevent_owner
BEFORE INSERT OR UPDATE OR DELETE ON cicd.Repository_Collaborator
FOR EACH ROW EXECUTE FUNCTION cicd.prevent_owner_change_func();