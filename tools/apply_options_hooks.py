#!/usr/bin/env python3
"""OPTION MODE's dash rows at X's native dash decisions, paired with LLE hooks.

$81:9742 is X1's second-tap check for double-tap dash and $81:8767 the wall
kick that reads the dash window ($0BFE). Generated blocks call
MmxOptionsHook on entry; the interpreter hooks in mmx_options.c cover the
interpreted paths."""
import argparse
from pathlib import Path
import re

MARKER = '/*MMX-OPTIONS*/'
PCS = ('819742', '818767')


def apply(text):
    text = ''.join(line for line in text.splitlines(keepends=True) if MARKER not in line)

    def inject(match):
        return match[0] + (f'    {MARKER} {{ extern void MmxOptionsHook(CpuState *, uint32_t); '
                           f'MmxOptionsHook(cpu, 0x{match["pc"]}); }}\n')
    return re.subn(r'    cpu_trace_block\(cpu, 0x(?P<pc>' + '|'.join(PCS) + r')\);\n'
                   r'(?:(?!cpu_trace_block)[\s\S])*?'
                   r'    cpu->coprocessor_master_cycles = cpu->master_cycles;\n', inject, text)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--gen-dir', type=Path, required=True)
    args = parser.parse_args()
    count = 0
    pattern = re.compile(r'cpu_trace_block\(cpu, 0x(?:' + '|'.join(PCS) + r')\)')
    for path in args.gen_dir.glob('*.c'):
        original = path.read_text(encoding='utf-8')
        if not pattern.search(original):
            continue
        updated, sites = apply(original)
        count += sites
        if updated != original:
            path.write_text(updated, encoding='utf-8', newline='\n')
    print(f'MMX option mode: {count} native dash hooks; interpreter hooks cover other paths')


if __name__ == '__main__':
    main()
