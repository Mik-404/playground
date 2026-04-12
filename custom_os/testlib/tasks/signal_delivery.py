import asyncio
from testlib import asserts
from testlib.testing import TestResult, TestBase, timeout

class TestSignalDelivery(TestBase):
    async def run(self):
        await self.build(make_extra_vars={
            "TEST_SIGNAL": "1"
        })

        async with asyncio.timeout(60):
            async with self.start_driver(memory="256m") as driver:
                await asserts.expect_success(driver)

        return TestResult.ok()
