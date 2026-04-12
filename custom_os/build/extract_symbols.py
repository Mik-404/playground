import sys
import subprocess
import io
from elftools.elf.elffile import ELFFile
from elftools.elf.sections import SymbolTableSection

demangle_path = sys.argv[2]

results = []

with open(sys.argv[1], "rb") as f:
    elf = ELFFile(f)
    syms = elf.get_section_by_name(".symtab")
    assert isinstance(syms, SymbolTableSection)
    all_syms = syms.iter_symbols()
    all_syms = filter(lambda x: x.entry.st_info.type not in ['STT_FILE', 'STT_NOTYPE'], all_syms)
    all_syms = sorted(all_syms, key=lambda x: x.entry.st_value + x.entry.st_size)
    all_syms = list(all_syms)

inp = ""

for sym in all_syms:
    inp += f"{sym.name}\n"

def decode(r):
    sym, demangled_line = r
    status, name = demangled_line.split("|", 1)
    if int(status) != 0:
        return sym.name
    return name

out = subprocess.check_output(demangle_path, input=inp.encode("ascii")).decode("ascii")
demangled = list(map(decode, zip(all_syms, out.splitlines())))

print('.section .ksyms,"a"')
print('.global _ksymdefs_start')
print('_ksymdefs_start:')

symstrs_pos = 0
for sym, demangled_name in zip(all_syms, demangled):
    print(f"    # {sym.name}")
    print(f"    # {sym.entry.st_info.type} {demangled_name}")
    print(f"    .quad {hex(sym.entry.st_value + sym.entry.st_size)}")
    print(f"    .quad {sym.entry.st_size}")
    print(f"    .quad {symstrs_pos}")
    print(f"    .quad {len(demangled_name)}")
    print()
    symstrs_pos += len(demangled_name) + 1

print('.global _ksymdefs_end')
print('_ksymdefs_end:')

print('.global _ksymstrs_start')
print('_ksymstrs_start:')
for demangled_name in demangled:
    escaped = demangled_name.replace('"', '\\"')
    print(f"    .asciz \"{escaped}\"")
