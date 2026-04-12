import asyncio
from testlib import asserts
from testlib.testing import TestResult, TestBase, timeout

class TestSlabAlloc(TestBase):
    async def run(self):
        await self.build(
            config_values={
                "TEST_SLAB_ALLOC": True,
                "DUPLICATE_PRINTK_TO_COM2": True,
            },
        )

        async with asyncio.timeout(120):
            async with self.start_driver(memory="128m") as driver:
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
                            asserts.fail("slab allocator returned pointer 0x%x twice", addr)
                        addrs.add(addr)
                    elif line.strip().endswith(b"done"):
                        break

                if total_allocated == 0:
                    return TestResult(code=TestResult.INTERNAL_ERROR, msg="total_allocated == 0, report this problem to the chat", score=0)

                await asserts.expect_success(driver)

        return TestResult.ok()
