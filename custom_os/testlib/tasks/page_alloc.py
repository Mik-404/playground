import asyncio
from testlib import asserts
from testlib.driver import QemuDriver
from testlib.testing import TestResult, TestBase, timeout


class TestPageAllocBasic(TestBase):
    async def run(self):
        await self.build(
            config_values={
                "TEST_PAGE_ALLOC_BASIC": True,
                "TEST_PAGE_ALLOC_TRACE_PTRS": True,
                "DUPLICATE_PRINTK_TO_COM2": True,
                "KASAN": False,
            },
        )

        async with self.start_driver(memory="256m") as driver:
            async with asyncio.timeout(30):
                addrs = set()
                total_allocated = 0

                while True:
                    line = await driver.serial_reader.readline()
                    line = line.strip()
                    if len(line) == 0:
                        await asserts.expect_success(driver)
                        return TestResult(code=TestResult.INCORRECT_SOLUTION, msg="EOF occurred while reading output from solution")
                    if line.startswith(b"[test] allocated ptr: "):
                        total_allocated += 1
                        addr = int(line[22:], 16)
                        if addr in addrs:
                            asserts.fail("page_alloc allocated page twice")
                        addrs.add(addr)
                    elif line.startswith(b"[test] free ptr: "):
                        addr = int(line[17:], 16)
                        if addr not in addrs:
                            asserts.fail("trying to free non-allocated pointer")
                        addrs.remove(addr)
                    elif line.strip().endswith(b"done"):
                        break

                if total_allocated == 0:
                    return TestResult(code=TestResult.INTERNAL_ERROR, msg="total_allocated == 0, report this problem to the chat", score=0)

                await asserts.expect_success(driver)

        return TestResult.ok()


class TestPageAllocFragmentation(TestBase):
    async def run(self):
        await self.build(
            config_values={
                "TEST_PAGE_ALLOC_FRAGMENTATION": True,
            },
        )

        async with asyncio.timeout(20):
            async with self.start_driver() as driver:
                await asserts.expect_success(driver)

        return TestResult.ok()
