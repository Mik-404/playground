import argparse
import os
import subprocess
import pathlib
import asyncio
import traceback
import tempfile
import shutil
from termcolor import colored
from testlib import asserts
from testlib.driver import QemuDriver
from testlib.testing import TestResult
from testlib.manytask import ManytaskClient

from testlib.tasks import multiboot
from testlib.tasks import lapic
from testlib.tasks import page_alloc
from testlib.tasks import slab_alloc
from testlib.tasks import pipes
from testlib.tasks import fpu_context
from testlib.tasks import signal_delivery

class Group:
    def __init__(self, name):
        self.name = name

class Task:
    def __init__(self, name, tests, max_score, all_or_nothing=True):
        self.name = name
        self.tests = tests
        self.all_or_nothing = all_or_nothing
        self.max_score = max_score

TASKS = [
    Task(
        name="multiboot",
        max_score=100,
        tests=[
            multiboot.TestMultiboot,
            multiboot.TestMultibootWithFakeMagic,
        ],
    ),
    Task(
        name="lapic",
        max_score=200,
        tests=[
            lapic.TestLapicCalibrated,
        ],
    ),
    Task(
        name="page-alloc",
        max_score=400,
        tests=[
            page_alloc.TestPageAllocBasic,
            page_alloc.TestPageAllocFragmentation,
        ],
    ),
    Task(
        name="slab-alloc",
        max_score=200,
        tests=[
            slab_alloc.TestSlabAlloc,
        ],
    ),
    Task(
        name="signal-delivery",
        max_score=200,
        tests=[
            signal_delivery.TestSignalDelivery,
        ]
    ),
    Task(
        name="fpu-context",
        max_score=200,
        tests=[
            fpu_context.TestFpuContext,
        ]
    ),
    Task(
        name="pipes",
        max_score=300,
        tests=[
            pipes.TestPipes,
        ],
    ),
]

def run_pre_build(tester):
    loop = asyncio.get_event_loop()
    try:
        fn = tester.pre_build()
        loop.run_until_complete(fn)
    except:
        traceback.print_exc()
        print(colored("Pre-build failure", "red"))
        exit(1)

def run_post_build(tester):
    if not hasattr(tester, "post_build"):
        return

    loop = asyncio.get_event_loop()
    try:
        fn = tester.post_build()
        loop.run_until_complete(fn)
    except:
        traceback.print_exc()
        print(colored("Post-build failure", "red"))
        exit(1)

async def _run_test(tester):
    try:
        return await asyncio.wait_for(tester.run(), timeout=300)
    except asyncio.TimeoutError:
        return TestResult(code=TestResult.TIMED_OUT, score=0)
    except asserts.BuildError as e:
        return TestResult(code=TestResult.BUILD_ERROR, msg=e.msg, score=0)
    except asserts.TestAssertionError as e:
        return TestResult(code=TestResult.INCORRECT_SOLUTION, msg=e.msg, score=0)
    except EOFError:
        return TestResult(code=TestResult.INCORRECT_SOLUTION, score=0)
    except:
        traceback.print_exc()
        return TestResult(code=TestResult.INTERNAL_ERROR, score=0)


def build_and_run_test(tester_cls, project_dir) -> TestResult:
    work_dir = tempfile.mkdtemp()
    try:
        work_dir = pathlib.Path(work_dir)
        tester = tester_cls(work_dir=work_dir)

        shutil.copytree(
            src=project_dir,
            dst=work_dir,
            ignore=shutil.ignore_patterns(
                ".git/**",
                ".cache",
                "*.venv",
                "*.o",
                "*.elf",
                "*.img",
                ".gen/**",
                "testlib/**",
            ),
            dirs_exist_ok=True,
        )

        return asyncio.run(_run_test(tester=tester))
    finally:
        if os.environ.get("KEEP_WORKDIR") is None:
            shutil.rmtree(work_dir)


class TaskTestResult:
    def __init__(self, task, failure, successful_tests, total_score):
        self.task = task
        self.failure = failure
        self.successful_tests = successful_tests
        self.total_score = total_score


def test_task(project_dir, task: Task) -> bool:
    required_tests_passed = True
    total_score = 0
    successful_tests = 0
    for test in task.tests:
        print(colored(f"> Running {test.__name__}", "white", attrs=["bold"]))
        testing_result = build_and_run_test(
            tester_cls=test,
            project_dir=project_dir,
        )
        total_score += testing_result.score
        failure = testing_result.code != TestResult.OK

        if testing_result.code == TestResult.OK:
            if testing_result.msg is not None:
                print(colored(f"OK: {testing_result.msg}", "green", attrs=["bold"]))
            else:
                print(colored(f"OK", "green", attrs=["bold"]))
            successful_tests += 1
            continue

        if testing_result.code == TestResult.TIMED_OUT:
            print(colored("Timed out", "yellow", attrs=["bold"]))
        elif testing_result.code == TestResult.INCORRECT_SOLUTION:
            print(colored("Incorrect solution" + f": {testing_result.msg}" if testing_result.msg else "", "red", attrs=["bold"]))
        elif testing_result.code == TestResult.INTERNAL_ERROR:
            print(colored("Internal error" + f": {testing_result.msg}" if testing_result.msg else "", "red", attrs=["bold"]))
        elif testing_result.code == TestResult.BUILD_ERROR:
            print(colored("Build error" + f": {testing_result.msg}" if testing_result.msg else "", "red", attrs=["bold"]))
        break

    if task.all_or_nothing:
        if successful_tests != len(task.tests):
            total_score = 0
            failure = True
        else:
            total_score = task.max_score
            failure = False
    else:
        if not required_tests_passed:
            total_score = 0
        failure = not required_tests_passed

    if total_score < 0 or total_score > task.max_score:
        print(colored(f"Invalid score from tester: {total_score}", "red", attrs=["bold"]))
        return TaskTestResult(task=task, failure=True, successful_tests=successful_tests, total_score=total_score)

    return TaskTestResult(task=task, failure=failure, successful_tests=successful_tests, total_score=total_score)


def grade_all(tasks_to_test: list[str] | None):
    project_dir = pathlib.Path(os.environ.get("CI_PROJECT_DIR", "."))
    os.chdir(project_dir)

    results = []
    tasks_to_test = tasks_to_test or list(map(lambda x: x.name, TASKS))
    tasks_to_test = set(tasks_to_test)
    for task in TASKS:
        if task.name not in tasks_to_test:
            continue
        results.append(test_task(project_dir=project_dir, task=task))

    failure = False
    for result in results:
        print(colored(f"{result.task.name}: {result.successful_tests}/{len(result.task.tests)} tests passed, {result.total_score} points", "red" if result.failure else "green", attrs=["bold"]))
        failure = failure or result.failure

    if os.environ.get("PUSH_RESULTS") is not None:
        manytask = ManytaskClient(os.environ["MANYTASK_URL"], os.environ["COURSE_NAME"], token=os.environ["TESTER_TOKEN"])
        for result in results:
            # TODO: get student's gitlab ID to allow teachers and assistants rebuild solutions
            gitlab_user_id = os.environ["GITLAB_USER_ID"]
            manytask.push_report(
                task_name=result.task.name,
                user_id=gitlab_user_id,
                score=result.total_score,
            )

    if failure:
        exit(1)


def main():
    parser = argparse.ArgumentParser(prog="grade.py", description="Utility for running tests for OS course")
    parser.add_argument("-t", "--task", action="append")
    args = parser.parse_args()
    grade_all(args.task)

if __name__ == "__main__":
    main()
