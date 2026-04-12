import asyncio
import os
import pathlib
import random
from qemu.qmp import QMPClient
from qemu.qmp.protocol import ConnectError
from elftools.elf.elffile import ELFFile
from elftools.elf.sections import SymbolTableSection

class QemuDriver:
    def __init__(self, work_dir: pathlib.Path, memory="128m", disk_img=None):
        self._work_dir = work_dir
        self.kernel_iso = work_dir / "kernel.iso"
        self.kernel_bin = work_dir / "kernel.elf"
        self._opened_kernel_bin = open(self.kernel_bin, "rb")
        self.elf_file = ELFFile(self._opened_kernel_bin)
        self._memory = memory
        self._disk_img = disk_img

    def get_kernel_symbol(self, name):
        syms = self.elf_file.get_section_by_name(".symtab")
        assert isinstance(syms, SymbolTableSection)
        return syms.get_symbol_by_name(name)

    async def _connect_unix(self, path):
        while True:
            await asyncio.sleep(0.5)
            try:
                return await asyncio.open_unix_connection(path, limit=2 * (1 << 20))
            except (FileNotFoundError, ConnectionRefusedError) as e:
                print(path, e)
                continue
            except:
                raise

    async def _connect_qmp(self, path):
        for _ in range(50):
            await asyncio.sleep(0.5)
            try:
                self.qmp = QMPClient("testing")
                await self.qmp.connect(path)
                break
            except ConnectError as e:
                print(path, e)
                continue
            except:
                raise

    async def __aenter__(self):
        cmd = [
            "qemu-system-x86_64",
            "-m", self._memory,
            "-device", "isa-debug-exit,iobase=0x501,iosize=0x2",
            "-qmp", f"unix:{self._work_dir}/qemu.sock,server",
            "-serial", "stdio",
            "-serial", f"unix:{self._work_dir}/serial.sock,server,nowait",
            "-monitor", "null",
            "-nographic",
            "-cdrom", self.kernel_iso,
            "-boot", "d",
            "-no-reboot",
            "-S",
        ]
        if self._disk_img is not None:
            cmd += ["-hda", self._disk_img]

        self._proc = await asyncio.create_subprocess_exec(
            *cmd,
            stderr=asyncio.subprocess.STDOUT,
        )

        try:
            await self._connect_qmp(f"{self._work_dir}/qemu.sock")
            self.serial_reader, self.serial_writer = await self._connect_unix(f"{self._work_dir}/serial.sock")
            await self.qmp.execute("cont")
        except:
            self._proc.kill()
            await self._proc.wait()
            raise

        return self

    async def wait(self):
        await self._proc.wait()

    async def __aexit__(self, exc_type, exc, tb):
        self.kill()
        await self._proc.wait()
        try:
            await self.qmp.disconnect()
        except:
            pass
        self._opened_kernel_bin.close()

    def kill(self):
        try:
            self._proc.kill()
        except ProcessLookupError:
            pass

    @property
    def exit_code(self):
        return self._proc.returncode

    async def check_shutdown(self):
        while not self.qmp.events.empty():
            ev = await self.qmp.events.get()
            if ev["event"] == "SHUTDOWN":
                return ev

    async def _wait_panic(self):
        pass

    async def read_virt_mem(self, addr: int, size: int, cpu=None):
        fname = f"/tmp/qemu-mem-{random.randint(0, 1e18)}"
        args = {"val": addr, "size": size, "filename": fname}
        if cpu is not None:
            args["cpu"] = cpu
        try:
            await self.qmp.execute("memsave", args)
            with open(fname, "rb") as f:
                return f.read()
        finally:
            try:
                os.unlink(fname)
            except:
                pass

    async def read_phys_mem(self, addr: int, size: int):
        fname = f"/tmp/qemu-mem-{random.randint(0, 1e18)}"
        try:
            await self.qmp.execute("pmemsave", {"val": addr, "size": size, "filename": fname})
            with open(fname, "rb") as f:
                return f.read()
        finally:
            try:
                os.unlink(fname)
            except:
                pass

    async def get_rip(self):
        r = await self.qmp.execute("human-monitor-command", {"command-line": "info registers"})
        for l in r.split("\n"):
            if l.startswith("RIP="):
                return int(l[4:20], 16)

    async def readline_with_prefix(self, prefix: bytes):
        while True:
            line = await self.serial_reader.readline()
            if line == b"":
                raise EOFError
            if line.startswith(prefix):
                return line
