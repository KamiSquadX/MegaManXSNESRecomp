#!/usr/bin/env python3
"""Build assets/axl/sprites/giga_screen.png from X8 16-bit's GigaCrash art.

    python tools/axl/make_giga_screen.py --x8 <X8 16-bit source root>
        --out assets/axl/sprites/giga_screen.png

GigaCrashExplosion.tscn shows Effects/giga_attack.png (4x4 cells of 398x224,
14 used, " tag" at 35 fps) as `red_bg`: half-transparent and modulated by
Color(0.227, 0.235, 0.686). The recomp blends the overlay itself, so this
keeps each cell's 4:3 middle (256 wide), applies the modulate, makes every
covered pixel opaque, and reduces the colours to 16 for the Axl sidecar.
"""
import argparse
import os

from PIL import Image

CELL_W, CELL_H, COLUMNS, FRAMES = 398, 224, 4, 14
MODULATE = (0.227451, 0.235294, 0.686275)
WIDTH = 256


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--x8', required=True)
    ap.add_argument('--out', required=True)
    args = ap.parse_args()
    src = Image.open(os.path.join(args.x8, 'Axl_mod/Player/Effects/giga_attack.png')).convert('RGBA')
    sheet = Image.new('RGBA', (WIDTH * COLUMNS, CELL_H * ((FRAMES + COLUMNS - 1) // COLUMNS)))
    left = (CELL_W - WIDTH) // 2
    for i in range(FRAMES):
        cx, cy = (i % COLUMNS) * CELL_W, (i // COLUMNS) * CELL_H
        cell = src.crop((cx + left, cy, cx + left + WIDTH, cy + CELL_H))
        px = cell.load()
        for y in range(CELL_H):
            for x in range(WIDTH):
                r, g, b, a = px[x, y]
                px[x, y] = (int(r * MODULATE[0]), int(g * MODULATE[1]), int(b * MODULATE[2]),
                            255 if a else 0)
        sheet.paste(cell, ((i % COLUMNS) * WIDTH, (i // COLUMNS) * CELL_H))
    alpha = sheet.getchannel('A')
    rgb = sheet.convert('RGB').quantize(colors=16, method=Image.Quantize.MEDIANCUT).convert('RGB')
    out = rgb.convert('RGBA')
    out.putalpha(alpha)
    out.save(args.out)
    print(f'{FRAMES} frames, {out.size[0]}x{out.size[1]}')


if __name__ == '__main__':
    main()
