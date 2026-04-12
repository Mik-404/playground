from testlib import asserts
from testlib.driver import QemuDriver
from testlib.testing import TestResult, TestBase


class TestMultiboot(TestBase):
    async def run(self):
        self.modify_file("linker.ld", lambda x: x.replace(b"ENTRY(_early_entry_point)", b"ENTRY(_fake_multiboot_start)"))
        await self.build(
            config_values={
                "TEST_MULTIBOOT": True,
            },
        )

        async with self.start_driver() as driver:
            await asserts.expect_success(driver)

        return TestResult.ok()


class TestMultibootWithFakeMagic(TestBase):
    async def run(self):
        self.modify_file("linker.ld", lambda x: x.replace(b"ENTRY(_early_entry_point)", b"ENTRY(_fake_multiboot_start)"))
        await self.build(
            config_values={
                "TEST_MULTIBOOT": True,
                "TEST_MULTIBOOT_FAKE_MAGIC": True,
            },
        )
        async with self.start_driver() as driver:
            await asserts.expect_panic(driver)

        return TestResult.ok()

# class TestMultibootWithFakeInfo(TestBase):
#     TIMEOUT_SEC = 20

#     def get_make_flags(self) -> dict[str, str]:
#         return {
#             "EXTRACFLAGS": "-D__REDIRECT_PRINTK_TO_SERIAL__",
#         }

#     async def run(self):
#         self.copy_files("multiboot_fake_info.S")
#         self.modify_file("linker.ld", lambda x: x.replace(b"ENTRY(_start)", b"ENTRY(_fake_start)"))
#         await self.build(
#             make_extra_args={ "EXTRACFLAGS": "-DPANIC_EXIT_QEMU", "TEST_MULTIBOOT": "1" },
#         )

#     async def test(self, driver: QemuDriver):
#         argv = await driver.readline_with_prefix(b"[argv]")
#         mmap_strs = [
#             (await driver.readline_with_prefix(b"[mmap]")).strip(),
#         ]
#         while True:
#             line = await driver.serial_reader.readline()
#             line = line.strip()
#             if line.startswith(b"[mmap]"):
#                 mmap_strs.append(line)
#             else:
#                 break

#         asserts.eq_verbose(
#             b'[argv] -test -string -passed=34142',
#             argv.strip(),
#             "argv doesn't match passed command line",
#         )

#         asserts.len_verbose(
#             4,
#             mmap_strs,
#             "multiboot memory map parsed incorrectly (incorrect number of regions)",
#         )

#         correct_ans = [
#             b'[mmap] ram 0xdeadbeef +94123',
#             b'[mmap] acpi 0xcafebabe +123',
#             b'[mmap] reserved 0xfffffff +552',
#             b'[mmap] total = 3',
#         ]
#         for expected, actual in zip(correct_ans, mmap_strs):
#             asserts.eq_verbose(
#                 expected,
#                 actual,
#                 msg="multiboot memory map parsed incorrectly",
#             )

#         return TestResult.ok(-1)
