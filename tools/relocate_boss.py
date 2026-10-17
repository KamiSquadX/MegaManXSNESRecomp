#!/usr/bin/env python3
"""Generate an address-only relocation descriptor for an X3 Maverick.

The descriptor tells src/mmx_boss_rush_relocate.c how to rebuild the boss's
original 65816 code inside X1's cart image at activation, from the user's own
X3 ROM:

* whole source code banks are copied into X1's unused LoROM padding banks
  ($30-$3F canonical; X1's 1.5 MB image is mirrored there and never read);
* the few data tables the boss reads through data bank $86 move into the
  $FF padding at the end of X1's own bank $86, because X1's helpers (damage,
  terrain, hitbox overlap) read hitboxes through that same data bank;
* every traced instruction that names something outside the boss is
  rewritten in place (same length): X3 engine helpers become X1 helpers,
  relocated copies, or small shims; X3 RAM becomes X1 RAM or a zero/sink
  area (reads return zero, ROM writes are ignored).

Anything the descriptor does not classify is an error: unknown helpers or
RAM never silently run X3 addresses inside X1.

Usage: relocate_boss.py <X3 USA ROM> <output .inc>
The output contains addresses and the original operand values used to
verify the user's ROM; it contains no graphics, audio or code bytes.
"""
import sys
from snes65816 import Rom, trace

X3_SHA256 = '65b03268afac296330e8ff8d60dd0825879e13ed658b37713c034a3bd074f1d7'

# X3 canonical bank -> X1 canonical bank (all inside X1's $30-$3F padding).
BANKS = {0x3f: 0x3f, 0x13: 0x33, 0x08: 0x38, 0x04: 0x34, 0x02: 0x32}

# Same-engine helpers with identical calling conventions in X1.
X1_HELPERS = {
    0x02d636: 0x8280b4,  # off-screen cull
    0x04c0f7: 0x8491be,  # terrain collision -> +$2B
    0x04be35: 0x849086,  # RNG
    0x04cb74: 0x849b43,  # take player-shot damage (A: 0 none, N dead)
    0x04cb31: 0x849b03,  # contact damage
    0x02d8de: 0x828358,  # allocate an enemy-projectile slot (X, Z set)
    0x02e13f: 0x84aca5,  # face the player
}
# X3 engine routines copied with the boss (object-local or X3 data only).
RELOCATED = {0x04b967, 0x04b94a,          # start / advance animation (X3 sets)
             0x02d7f3, 0x02d78e, 0x02d6f8,  # velocity integration
             0x04cc5c,                    # hitbox overlap with object X
             0x02df64,                    # damage flash timer
             0x04d8d5,                    # stop when the player is dead
             0x13f7d6, 0x13f7fc}          # Rhino's palette rotate / fade back
# X3-only behavior Boss Rush replaces. Values name shim bodies below.
SHIMS = {
    0x0084bc: 'rtl',     # music request (Rush owns the battle song)
    0x00850f: 'rtl',     # SPC bank upload (boss sounds come later)
    0x00b1dc: 'rtl',     # Doppler refight graphics
    0x018000: 'rtl',     # positional sound effect
    0x01802b: 'rtl',     # sound effect
    0x04d1ca: 'rtl',     # lock player for the intro / pin
    0x04d1ef: 'rtl',     # release player
    0x04d580: 'rtl',     # screen shake
    0x02df8a: 'rtl',     # dust effect
    0x02e0ba: 'rtl',     # stock boss explosion (Rush draws its own)
    0x02e0e6: 'rtl',
    0x04d8cf: 'rtl',     # weapon award checks (never reached in Rush)
    0x04d8e1: 'rtl',
    0x04d6e0: 'rtl',     # projectile death sparkle
    0x02ddfe: 'rtl',     # weapon stun reaction
    0x02e016: 'clc',     # palette fade step: report finished
    0x02db3f: 'combat',  # generic hurt reaction: resume combat
    0x02d928: 'delete',  # delete this object
    0x02d933: 'delete',
    0x02e15c: 'init',    # class init row (generated from the ROM)
}
# X3 RAM -> X1 RAM. The $09xx/$0Axx player block is X1's $0Bxx/$0Cxx + $1D0.
RAM = {0x09cb: 0x0b9b, 0x09cc: 0x0b9c, 0x09da: 0x0baa, 0x09dd: 0x0bad, 0x09e0: 0x0bb0,
       0x09ff: 0x0bcf, 0x0a03: 0x0bd3, 0x1e5d: 0x1e4d, 0x1e60: 0x1e50, 0x1f1f: 0x1f0e}
# X3-only globals: reads see zero, writes land in ROM and are ignored.
ZERO = {0x0040, 0x00a1, 0x0a43, 0x1f18, 0x1f2f, 0x1f36, 0x1f54, 0x1f55, 0x1f56, 0x1f57,
        0x1fae, 0x1fca, 0x1fda, 0x1fe0, 0x049e, 0x0480, 0x0482, 0x0484, 0x04a0}
ZERO_LONG = {0x001f39, 0x7fcac0}
# 16-bit immediates that are RAM object pointers.
IMMEDIATE = {0x09d8: 0x0ba8}

# X1 bank $86 padding ($FA44-$FFFF is $FF fill in the USA ROM).
PAD_SHIMS, PAD_DATA, PAD_ZERO = 0x86fa80, 0x86fb00, 0x86fe00


class Boss:
    def __init__(self, name, roster_id, body, children, x3_class, data):
        self.name, self.roster_id, self.body = name, roster_id, body
        self.children, self.x3_class, self.data = children, x3_class, data


RHINO = Boss('rhino', 23, 0x3fe5e4, {0x26: 0x08ab9b}, 0x55,
             # $86 tables read through the data bank: hitboxes, drill spawn
             # tables and the shared angle table (indices $00-$70).
             [(0x86ec87, 10), (0x86efb8, 0x35), (0x86e18e, 0x74)])


def bank_map(address):
    canonical = (address >> 16) & 0x7f
    if canonical not in BANKS:
        return None
    return (address & 0x80ffff) | (BANKS[canonical] << 16)


def shim_code(kind, init):
    if kind == 'rtl':
        return [0x6b]
    if kind == 'clc':
        return [0x18, 0x6b]
    if kind == 'combat':
        return [0x08, 0xe2, 0x20, 0xa9, 0x02, 0x85, 0x01, 0x28, 0x6b]
    if kind == 'delete':
        return [0x08, 0xe2, 0x20, 0x64, 0x00, 0x28, 0x6b]
    if kind == 'init':
        # PHP; SEP #$30; clear $30/$1A-$1F/$18/$10; $01=2; $26/$27/$28/$16
        # from the class row (filled by the builder); PLP; RTL.
        code = [0x08, 0xe2, 0x30, 0x64, 0x30]
        for f in (0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x18, 0x10):
            code += [0x64, f]
        code += [0xa9, 0x02, 0x85, 0x01]
        for field in (0x26, 0x27, 0x28, 0x16):
            init.append((len(code) + 1, field))
            code += [0xa9, 0x00, 0x85, field]
        return code + [0x28, 0x6b]
    raise ValueError(kind)


def build(rom, boss):
    stop = lambda t: (t & 0x7fffff) in X1_HELPERS or (t & 0x7fffff) in SHIMS
    entries = [boss.body, *boss.children.values(), *RELOCATED]
    seen, tables, calls = trace(rom, entries, stop=stop)
    errors, patches, copies, blobs = [], [], [], []

    # Data tables into X1's $86 padding, keeping each table's layout.
    data_map, cursor = [], PAD_DATA
    for start, length in boss.data:
        data_map.append((start, length, cursor))
        copies.append((start, cursor, length))
        cursor += length
    if cursor > PAD_ZERO:
        errors.append('data tables overflow the $86 padding')

    def data_target(offset):
        for start, length, dest in data_map:
            if start & 0xffff <= offset < (start & 0xffff) + length:
                return (dest & 0xffff) + offset - (start & 0xffff)
        return None

    # Shims, one body per kind.
    shim_at, cursor, init_fields = {}, PAD_SHIMS, []
    for kind in sorted(set(SHIMS.values())):
        fields = []
        code = shim_code(kind, fields)
        shim_at[kind] = cursor
        blobs.append((cursor, code))
        if kind == 'init':
            row = 0x86e28e + (boss.x3_class - 1) * 5
            column = {0x16: 0, 0x26: 2, 0x27: 3, 0x28: 4}
            init_fields = [(cursor + at, row + column[field]) for at, field in fields]
        cursor += len(code)
    if cursor > PAD_DATA:
        errors.append('shims overflow their padding')

    def call_target(target):
        t = target & 0x7fffff
        if t in X1_HELPERS:
            return X1_HELPERS[t]
        if t in SHIMS:
            return shim_at[SHIMS[t]]
        if t in RELOCATED or any(t == (e & 0x7fffff) for e in (boss.body, *boss.children.values())):
            return bank_map(target)
        mapped = bank_map(target)
        if mapped is not None and target in seen:
            return mapped
        return None

    for a, insn in sorted(seen.items()):
        if bank_map(a) is None:
            errors.append(f'${a:06X}: traced code outside the copied banks')
            continue
        dest = bank_map(a)
        operand = dest + 1 if (dest & 0xffff) != 0xffff else None
        v, md, name = insn.value, insn.mode, insn.name

        def patch(size, value):
            patches.append((operand, size, v, value))

        if name in ('JSL', 'JML') and md == 'long':
            target = call_target(v)
            if target is None:
                errors.append(f'${a:06X}: unclassified call ${v:06X}')
            elif target != v:
                patch(3, target)
        elif md in ('long', 'long,x'):
            if v in ZERO_LONG:
                patch(3, PAD_ZERO | (v & 0x1ff))
            elif bank_map(v) is not None:
                if bank_map(v) != v:
                    patch(3, bank_map(v))
            else:
                errors.append(f'${a:06X}: unclassified long ${v:06X}')
        elif md in ('abs', 'abs,x', 'abs,y') and name not in ('JSR', 'JMP', 'PEA'):
            if v >= 0x8000:
                moved = data_target(v)
                if moved is None:
                    errors.append(f'${a:06X}: data read ${v:04X} outside relocated tables')
                elif moved != v:
                    patch(2, moved)
            elif md != 'abs' and v < 0x40:
                pass  # object field through X/Y
            elif md == 'abs' and v < 0x10:
                pass  # direct-page-style scratch shared by both engines
            elif v in RAM:
                patch(2, RAM[v])
            elif v in ZERO:
                patch(2, (PAD_ZERO & 0xffff) | (v & 0x1ff))
            else:
                errors.append(f'${a:06X}: unclassified RAM ${v:04X} ({name})')
        elif md in ('immm', 'immx') and insn.size == 3 and v in IMMEDIATE:
            patch(2, IMMEDIATE[v])
        elif md in ('immm', 'immx') and insn.size == 3:
            moved = data_target(v)
            if moved is not None and moved != v and 0x8000 <= v:
                patch(2, moved)  # hitbox pointer immediates
        elif md in ('immm', 'imm8') and insn.size == 2 and name == 'LDA':
            nxt = seen.get((a & 0xff0000) | ((a + 2) & 0xffff))
            if nxt and nxt.name == 'PHA' and v in BANKS and BANKS[v] != v:
                patch(1, BANKS[v])
            if nxt and nxt.name == 'STA' and nxt.mode == 'dp' and nxt.value == 0x20:
                # Split hitbox pointer: LDA #lo; STA $20; LDA #hi; STA $21.
                hi = seen.get((a & 0xff0000) | ((a + 4) & 0xffff))
                if hi and hi.name == 'LDA' and hi.size == 2:
                    moved = data_target(hi.value << 8 | v)
                    if moved is None:
                        errors.append(f'${a:06X}: split pointer ${hi.value:02X}{v:02X} not relocated')
                    elif moved != (hi.value << 8 | v):
                        patches.append((operand, 1, v, moved & 0xff))
                        patches.append((bank_map(hi.address) + 1, 1, hi.value, moved >> 8))

    if errors:
        raise SystemExit('Relocation failed:\n  ' + '\n  '.join(errors))
    banks = sorted({(a >> 16) & 0x7f for a in seen})
    return banks, patches, copies, blobs, init_fields, len(seen)


def emit(path, boss, banks, patches, copies, blobs, init_fields, count):
    out = [f'/* Generated by tools/relocate_boss.py ({boss.name}); do not edit.',
           ' * Addresses and original operand values only: the builder copies the',
           ' * code and data from the user\'s X3 ROM at activation. */',
           f'static const MmxRelocBank k_{boss.name}_banks[]={{']
    out += [f'  {{0x{b:02x},0x{BANKS[b]:02x}}},' for b in banks]
    out += ['};', f'static const MmxRelocCopy k_{boss.name}_copies[]={{']
    out += [f'  {{0x{s:06x},0x{d:06x},{n}}},' for s, d, n in copies]
    out += [f'  {{0x{s:06x},0x{d:06x},1}},' for d, s in init_fields]
    out += ['};', f'static const uint8_t k_{boss.name}_shim_code[]={{']
    for at, code in blobs:
        out.append(f'  /* ${at:06X} */ ' + ','.join(f'0x{b:02x}' for b in code) + ',')
    out += ['};', f'static const MmxRelocBlob k_{boss.name}_blobs[]={{']
    offset = 0
    for at, code in blobs:
        out.append(f'  {{0x{at:06x},{offset},{len(code)}}},')
        offset += len(code)
    out += ['};', f'static const MmxRelocPatch k_{boss.name}_patches[]={{']
    out += [f'  {{0x{a:06x},{n},0x{old:06x},0x{new:06x}}},' for a, n, old, new in sorted(patches)]
    out += ['};',
            f'/* {count} traced instructions, {len(patches)} rewritten operands. */',
            f'enum {{ k_{boss.name}_roster_id={boss.roster_id},'
            f' k_{boss.name}_body=0x{bank_map(boss.body) | 0x800000:06x} }};']
    out += [f'static const MmxRelocChild k_{boss.name}_children[]={{']
    out += [f'  {{0x{c:02x},0x{bank_map(a) | 0x800000:06x}}},' for c, a in boss.children.items()]
    out += ['};', '']
    with open(path, 'w', newline='\n') as f:
        f.write('\n'.join(out))


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    rom = Rom(sys.argv[1], X3_SHA256)
    result = build(rom, RHINO)
    emit(sys.argv[2], RHINO, *result)
    print(f'{RHINO.name}: {result[5]} instructions, {len(result[1])} patches -> {sys.argv[2]}')


if __name__ == '__main__':
    main()
