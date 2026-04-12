import requests
import json
import time
from datetime import datetime

class ManytaskRequestFailedError(Exception):
    pass


class PushFailedError(ManytaskRequestFailedError):
    pass


class GetFailedError(ManytaskRequestFailedError):
    pass

class ManytaskClient:
    def __init__(self, url, course_name, token):
        self._url = url
        self._token = token
        self._course_name = course_name

    def push_report(
        self,
        task_name: str,
        user_id: int,
        score: float,
        files=None,
        send_time=None,
        check_deadline: bool = True,
        use_demand_multiplier: bool = True,
    ):
        data = {
            'task': task_name,
            'user_id': user_id,
            'score': score,
            'check_deadline': check_deadline,
            'use_demand_multiplier': use_demand_multiplier,
        }
        if send_time:
            data['commit_time'] = send_time

        response = None
        for _ in range(3):
            response = requests.post(url=f'{self._url}/api/{self._course_name}/report', data=data, headers={"Authorization": f"Bearer {self._token}"}, files=files)

            if response.status_code < 500:
                break
            time.sleep(1.0)
        assert response is not None

        if response.status_code >= 500:
            response.raise_for_status()
            assert False, 'Not Reachable'
        elif response.status_code >= 400:
            # Client error often means early submission
            raise PushFailedError(f'{response.status_code}: {response.text}')
        else:
            try:
                result = response.json()
                result_commit_time = result.get('commit_time', None)
                result_submit_time = result.get('submit_time', None)
                demand_multiplier = float(result.get('demand_multiplier', 1))
                return result['username'], result['score'], result_commit_time, result_submit_time, demand_multiplier
            except (json.JSONDecodeError, KeyError) as e:
                raise PushFailedError('Unable to decode response') from e
