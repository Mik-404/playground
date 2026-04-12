import asyncio
import time
from testlib import asserts
from testlib.driver import QemuDriver
from testlib.testing import TestResult, TestBase, timeout


class TestLapicCalibrated(TestBase):
    async def run(self):
        await self.build(
            config_values={
                "TEST_LAPIC_TIMER": True,
            },
        )

        async with self.start_driver() as driver:
            async with asyncio.timeout(60):
                tick = await driver.serial_reader.readexactly(1)
                asserts.eq_verbose(b"@", tick, f"garbage detected in COM2 output: {tick}")
            ticks = []
            async with asyncio.timeout(10):
                while len(ticks) < 600:
                    async with asyncio.timeout(1):
                        tick = await driver.serial_reader.readexactly(1)
                        ticks.append(time.time())
                        asserts.eq_verbose(b"@", tick, f"garbage detected in COM2 output: {tick}")

            total_diff = 0
            for i in range(1, len(ticks)):
                total_diff += ticks[i] - ticks[i - 1]

            avg_interval = total_diff / (len(ticks) - 1)

            if abs(avg_interval - 0.005) >= 0.001:
                asserts.fail(f"Timer interval is ≈{round(avg_interval * 1000, 5)} ms, should be 5 ms")

        return TestResult.ok()
