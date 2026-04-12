import sys
from struct import unpack, pack
from elftools.elf.elffile import ELFFile

SYS_max = 21

for fname in sys.argv[1:]:
    with open(fname, "rb") as f:
        elf = ELFFile(f)
        tab = elf.get_section_by_name("syscalls_table")
        data = tab.data()

parsed = []
off = 0
while off < tab.data_size:
    parsed.append(unpack("<QQ", data[off:off + 16]))
    off += 16
parsed.sort()

assert len(parsed) == SYS_max, "incomplete syscall table"

for i in range(len(parsed)):
    assert parsed[i][0] == i, f"syscall #{i} is missing"

new_data = []
for df in parsed:
    new_data.append(pack("<QQ", *df))
new_data = b"".join(new_data)

with open(fname, "r+b") as f:
    f.seek(tab.header.sh_offset)
    f.write(new_data)
