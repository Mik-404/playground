import functools
from .asserts import fail, BuildError
import pathlib
import inspect
import os
import shutil
import jinja2
import subprocess
import asyncio
from termcolor import colored
from .driver import QemuDriver
import yaml
import os

class TestResult:
    OK = 0
    TIMED_OUT = 1
    INCORRECT_SOLUTION = 2
    INTERNAL_ERROR = 3
    BUILD_ERROR = 4

    def __init__(self, code, msg=None, score=None):
        self.code = code
        self.score = score
        self.msg = msg

    @staticmethod
    def ok(score=0, msg=None):
        return TestResult(code=TestResult.OK, score=score, msg=msg)


class TestBase:
    def __init__(self, work_dir):
        self.work_dir = work_dir

    def start_driver(self, memory="512m"):
        disk_img = None
        if os.path.exists(self.work_dir / "disk.img"):
            disk_img = self.work_dir / "disk.img"
        return QemuDriver(
            work_dir=self.work_dir,
            disk_img=disk_img,
            memory=memory,
        )

    async def run_driver(self, **kwargs):
        async with self.start_driver(**kwargs) as driver:
            await driver.wait()
            return driver

    async def build(self, config_values={}, make_extra_vars={}):
        vals = {
            "TARGET_ARCH": "x86",
            "TRACE_PAGE_FAULTS": False,
            "OPTIMIZE_LEVEL": "3",
            "KASAN": True,
            "COMPILE_STUBS": False,
        }
        vals.update(config_values)
        vals.update({"TESTING_IN_QEMU": True})
        with open(self.work_dir / "config_values.yaml", "w") as f:
            vals = yaml.dump(vals, f, Dumper=yaml.Dumper)

        sandbox_type = os.getenv('SANDBOX', 'docker')
        if sandbox_type == 'docker':
            uid = os.getuid()
            gid = os.getgid()
            cmd = [
                "docker",
                "run",
                "-v", f"{self.work_dir}:/opt/os/work",
                "--user", f"{uid}:{gid}",
                "--network", "none",
                "--rm",
                "docker.gitlab.carzil.ru/os-advanced/public-images/base-image:latest",
            ]
        elif sandbox_type == "minijail":
            cmd = [
                "minijail0",
                "-u", "nobody", # set user
                "-g", "nobody", # set group
                "-G",           # inherit supplementary groups
                "-p",           # new PID namespace
                "-N",           # new cgroup namespace
                "-e",           # new network namespace
                "--env-reset",  # reset environment variables
                "--env-add", "PATH=/opt/os/cross/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin",
                "--logging=stderr",
            ]
        else:
            raise ValueError(f"invalid type of sandbox '{sandbox_type}'")

        cmd.extend([
            "/bin/bash",
            "-c",
            "make clean && make -j$(nproc)",
        ])
        if os.environ.get("USE_CCACHE") is not None:
            make_extra_vars["CCACHE"] = "ccache"
        if len(make_extra_vars) > 0:
            cmd[-1] += " " + " ".join(map(lambda i: f"{i[0]}='{i[1]}'", make_extra_vars.items()))

        print(colored(f">> running {cmd}", "grey", attrs=["bold"]))

        proc = await asyncio.subprocess.create_subprocess_exec(
            *cmd,
            stdout=None,
            stderr=None,
            cwd=self.work_dir,
        )
        await proc.wait()
        ec = proc.returncode

        if ec != 0:
            raise BuildError()

    def test_file_path(self, path):
        return pathlib.Path(os.path.dirname(inspect.getmodule(self.__class__).__file__)) / "files" / path

    def modify_file(self, fname, fn):
        with open(self.work_dir / fname, "rb") as f:
            res = fn(f.read())
        with open(self.work_dir / fname, "wb") as f:
            f.write(res)

    def render_template(self, template, extra_filters={}, context={}, dst=None):
        env = jinja2.Environment()
        env.filters.update(extra_filters)
        with open(self.test_file_path(template)) as f:
            t = env.from_string(f.read())

        if dst is None:
            dst = self.work_dir / template
        else:
            dst = self.work_dir / dst
        with open(dst, "w") as f:
            rendered = t.render(**context)
            f.write(rendered)

    def copy_files(self, *fnames):
        for f in fnames:
            shutil.copyfile(self.test_file_path(f), self.work_dir / f)

    def copy_files2(self, *fnames, dst=None):
        assert dst is not None
        for f in fnames:
            shutil.copyfile(self.test_file_path(f), self.work_dir / dst / os.path.basename(f))

    async def execute(self, cmd):
        docker_cmd = [
            "docker",
            "run",
            "-v", f"{self.work_dir}:/opt/mipt-os/work",
            "--network", "none",
            "--rm",
            "docker.carzil.ru/mipt-os/images/testenv:latest",
        ] + cmd

        proc = await asyncio.create_subprocess_exec(
            *docker_cmd,
            stderr=asyncio.subprocess.STDOUT,
        )
        await proc.wait()
        if proc.returncode != 0:
            fail("external command failed")

    def get_make_flags(self) -> dict[str, str]:
        return {}


def timeout(t):
    def wrapper(func):
        @functools.wraps(func)
        async def wraped(*args, **kwargs):
            async with asyncio.timeout(t):
                return await func(*args, **kwargs)
        return wraped
    return wrapper
