from elftools.elf.elffile import ELFFile
from elftools.elf.sections import SymbolTableSection

class BuildError(Exception):
    def __init__(self, msg="", *args, **kwargs):
        super().__init__(self, msg, *args, **kwargs)
        self.msg = msg

class TestAssertionError(Exception):
    def __init__(self, msg="", *args, **kwargs):
        super().__init__(self, msg, *args, **kwargs)
        self.msg = msg


def eq(a, b, msg=None):
    if a != b:
        raise TestAssertionError(msg)


def eq_verbose(expected, actual, msg=None):
    if expected != actual:
        m = f"expected: {repr(expected)}\nactual: {repr(actual)}"
        if msg is not None:
            m = f"{msg}:\n" + m
        raise TestAssertionError(m)


def fail(msg):
    raise TestAssertionError(msg)


def true_verbose(actual, msg):
    if not actual:
        raise TestAssertionError(msg)


def len_verbose(expected, obj, msg):
    if len(obj) != expected:
        raise TestAssertionError(f"{msg}:\nexpected:{expected}\nactual:{len(obj)}")


async def in_panic(driver):
    rip = await driver.get_rip()

    syms = driver.elf_file.get_section_by_name(".symtab")
    assert isinstance(syms, SymbolTableSection)
    panic = syms.get_symbol_by_name("__panic")
    assert panic is not None and len(panic) > 0
    panic = panic[0]
    if rip <= panic.entry.st_value or rip >= panic.entry.st_value + panic.entry.st_size:
        raise TestAssertionError("kernel not running inside __panic")

    # check hlt opcode (f4)
    mem = await driver.read_phys_mem(rip - 1, 1)
    eq(mem, b"\xf4", "CPU is not executing HLT instruction")


async def expect_success(driver):
    await driver.wait()
    ev = await driver.check_shutdown()
    if ev is not None and ev["data"]["guest"] and ev["data"]["reason"] != "guest-shutdown":
        raise TestAssertionError(f"unexpected QEMU shutdown, reason: {ev['data']}")
    eq_verbose(123, driver.exit_code, "non-successful QEMU exit code")


async def expect_panic(driver):
    await driver.wait()
    ev = await driver.check_shutdown()
    if ev is not None and ev["data"]["guest"] and ev["data"]["reason"] != "guest-shutdown":
        raise TestAssertionError(f"unexpected QEMU shutdown, reason: {ev['data']}")
    eq_verbose(75, driver.exit_code, "expected panic")
