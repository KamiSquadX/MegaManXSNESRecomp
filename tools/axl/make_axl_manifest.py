#!/usr/bin/env python3
"""Build tools/axl/axl_manifest.json from the X8 16-bit Axl SpriteFrames.

    python tools/axl/make_axl_manifest.py --x8 <X8 16-bit source root>
        --sprites assets/axl/sprites --out tools/axl/axl_manifest.json

The manifest is the Saber sidecar format (tools/saber/convert_saber_zero.py),
so the game parses Axl with the same loader as Saber Zero. Only the run is
needed when the donor art changes; the build uses the committed manifest.

Each frame is trimmed to its opaque pixels. `origin` is where the trimmed
rectangle sits in the 128x128 player canvas whose anchor (64, 64) is the
AnimatedSprite position: Godot centres the 64x64 cell there, 4 px above the
body origin (PlayerAxl.tscn, node animatedSprite).
"""
import argparse
import hashlib
import json
import os
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(__file__))
from parse_godot_frames import parse  # noqa: E402

# Animation ids are the game's (src/mmx_axl.h): (id, source, name, only,
# texture swap, centred). Sources are X8 16-bit SpriteFrames: `axl` the
# player, `giga` GigaCrash's illusion (its floor set is a binary resource, so
# the floor frames reuse the air set's regions on Giga.png), `copy` the copy
# shot. `only` keeps a subset of an animation's frames (None keeps them all);
# the shot sets hold one frame per aim direction, up to down, and the game
# picks the step by aim. Centred frames (projectiles) sit on the anchor; the
# body frames sit 4 px above it, as AnimatedSprite does in PlayerAxl.tscn.
SOURCES = {
    'axl': 'Axl_mod/Player/Sprites/axl.tres',
    'giga': 'Axl_mod/Player/Projectiles/BossWeapons/GigaCrash/gigacrash_air.tres',
    'copy': 'Axl_mod/Player/Projectiles/Copy Bullet.tscn',
}
FLOOR = {'Giga_Air.png': 'Giga.png'}
ANIMATIONS = [
    (1, 'axl', 'idle'), (2, 'axl', 'walk'), (3, 'axl', 'jump'), (4, 'axl', 'fall'),
    (5, 'axl', 'dash'), (6, 'axl', 'dodge'), (7, 'axl', 'hover'), (8, 'axl', 'slide'),
    (9, 'axl', 'walljump'), (10, 'axl', 'shot'), (11, 'axl', 'shot_air'),
    (12, 'axl', 'shot_slide'), (13, 'axl', 'damage'), (14, 'axl', 'stairs'),
    (15, 'axl', 'beam_in'), (16, 'axl', 'victory'), (17, 'axl', 'crouch'),
    (18, 'axl', 'recover'), (19, 'axl', 'walk_start'), (20, 'axl', 'shot_recover'),
    (21, 'axl', 'stairs_start'), (22, 'axl', 'stairs_end'), (23, 'axl', 'beam_out'),
    (24, 'axl', 'diagonal_dash'),
    (25, 'giga', 'intro'), (26, 'giga', 'fire_loop', None, FLOOR),
    (27, 'giga', 'fire_idle', None, FLOOR), (28, 'axl', 'recover'),
    (29, 'giga', 'intro'), (30, 'giga', 'fire_loop'), (31, 'giga', 'fire_idle'),
    (32, 'giga', 'fire_end'),
    (33, 'copy', 'Tag', None, None, True),
    (34, 'bullet', 'bullet', None, None, True),
    (35, 'giga_screen', 'giga_screen', None, None, True),
    (36, 'axl', 'beam'),
    (37, 'rotated', 'dash_up'), (38, 'rotated', 'dash_down'), (39, 'rotated', 'dash_down_tilt'),
    (40, 'rotated', 'bullet_up', None, None, True), (41, 'rotated', 'bullet_up_tilt', None, None, True),
    (42, 'rotated', 'bullet_down_tilt', None, None, True), (43, 'rotated', 'bullet_down', None, None, True),
    (44, 'hud', 'badge', None, None, True), (45, 'hud', 'life', None, None, True),
    (46, 'hud', 'pause'),
]
# Turned copies of the dash and bullet art, as X8 rotates the sprite
# (AirDashAxl.gd: up -90, down +90, down-tilt +45 degrees; bullets along
# their aim). Godot turns clockwise for positive degrees. Nearest-neighbour
# rotation keeps the palette; written to the sprite folder as ROTATED.
ROTATED = 'axl_rotated.png'
ROTATIONS = [  # name, source animation, frames, Godot degrees, cell size
    ('dash_up', ('axl', 'dash'), [1], -90, 96),
    ('dash_down', ('axl', 'dash'), [1], 90, 96),
    ('dash_down_tilt', ('axl', 'dash'), [1], 45, 96),
    ('bullet_up', ('bullet', 'bullet'), [0, 1, 2], -90, 32),
    ('bullet_up_tilt', ('bullet', 'bullet'), [0, 1, 2], -45, 32),
    ('bullet_down_tilt', ('bullet', 'bullet'), [0, 1, 2], 45, 32),
    ('bullet_down', ('bullet', 'bullet'), [0, 1, 2], 90, 32),
]


def build_rotated(anims, sprites):
    """Draw ROTATED and return its SpriteFrames-like animations."""
    cells = []
    for name, (key, anim), frames, degrees, size in ROTATIONS:
        a = anims[key][anim]
        for i in frames:
            path, (x, y, w, h) = a['frames'][i]
            src = Image.open(os.path.join(sprites, path.rsplit('/', 1)[-1])).convert('RGBA')
            cell = src.crop((x, y, x + w, y + h)).rotate(-degrees, resample=Image.NEAREST, expand=True)
            cells.append((name, a['speed'], cell, size))
    width = sum(size for *_, size in cells)
    sheet = Image.new('RGBA', (width, 96))
    out, at = {}, 0
    for name, speed, cell, size in cells:
        sheet.paste(cell, (at + (size - cell.width) // 2, (size - cell.height) // 2))
        out.setdefault(name, {'frames': [], 'speed': speed, 'loop': False})
        out[name]['frames'].append((ROTATED, (at, 0, size, size)))
        at += size
    sheet.save(os.path.join(sprites, ROTATED))
    return out

# Art with no SpriteFrames of its own: the pistol bullet strip, the
# GigaCrash screen overlay that tools/axl/make_giga_screen.py prepares, and
# the HUD: the health bar's A badge (the bottom 16 rows of Axl_bar.png), the
# pause menu's life head and portrait (HUD/Pause/Axl_base.png as Axl_pause.png).
RAW = {
    'giga_screen': {'giga_screen': {'frames': [('giga_screen.png', ((i % 4) * 256, (i // 4) * 224, 256, 224))
                                               for i in range(14)],
                                    'speed': 35.0, 'loop': True}},
    'bullet': {'bullet': {'frames': [('BulletAxl.png', (x, 0, 16, 16)) for x in (0, 16, 48)],
                          'speed': 20.0, 'loop': False}},
    'hud': {'badge': {'frames': [('Axl_bar.png', (0, 6, 14, 16))], 'speed': 0.0, 'loop': False},
            'life': {'frames': [('lives_axl.png', (0, 0, 18, 16))], 'speed': 0.0, 'loop': False},
            'pause': {'frames': [('Axl_pause.png', (0, 0, 64, 64))], 'speed': 0.0, 'loop': False}},
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--x8', required=True, help='Mega Man X8 16-bit source root')
    ap.add_argument('--sprites', required=True)
    ap.add_argument('--out', required=True)
    args = ap.parse_args()
    anims = {key: parse(os.path.join(args.x8, path)) for key, path in SOURCES.items()}
    anims.update(RAW)
    anims['rotated'] = build_rotated(anims, args.sprites)
    sources, frames, out_anims = {}, {}, []
    images = {}

    def source_id(name):
        if name not in sources:
            path = os.path.join(args.sprites, name)
            data = open(path, 'rb').read()
            sources[name] = {'id': len(sources) + 1, 'file': name,
                             'sha256': hashlib.sha256(data).hexdigest()}
            images[name] = Image.open(path).convert('RGBA')
        return sources[name]['id']

    def frame_id(path, rect, swap, centred):
        name = path.rsplit('/', 1)[-1]
        name = (swap or {}).get(name, name)
        key = (name, rect, centred)
        if key in frames:
            return frames[key]['id']
        sid = source_id(name)
        x, y, w, h = rect
        cell = images[name].crop((x, y, x + w, y + h))
        box = cell.getchannel('A').point(lambda a: 255 if a >= 128 else 0).getbbox()
        if not box:
            box = (0, 0, 1, 1)
        l, t, r, b = box
        frames[key] = {
            'id': 1000 + len(frames) + 1, 'source': sid,
            'body': {'rect': [x + l, y + t, r - l, b - t],
                     'origin': [64 - w // 2 + l, 64 - (0 if centred else 4) - h // 2 + t]},
            'source_frame': len(frames)}
        return frames[key]['id']

    for entry in ANIMATIONS:
        aid, key, name = entry[:3]
        only = entry[3] if len(entry) > 3 else None
        swap = entry[4] if len(entry) > 4 else None
        centred = entry[5] if len(entry) > 5 else False
        a = anims[key][name]
        steps = list(a['frames'])
        if only:
            steps = [steps[i] for i in only]
        ticks = max(1, round(60.0 / a['speed'])) if a['speed'] > 0 else 1
        out_steps = []
        for path, rect in steps:
            fid = frame_id(path, rect, swap, centred)
            if out_steps and out_steps[-1]['frame'] == fid:
                out_steps[-1]['ticks'] += ticks
            else:
                out_steps.append({'frame': fid, 'ticks': ticks})
        out_anims.append({'id': aid, 'facing_xor': 0, 'steps': out_steps})

    manifest = {
        'format': 'mmx-saber-manifest', 'version': 1,
        'alpha_threshold': 128,
        'sources': sorted(sources.values(), key=lambda s: s['id']),
        'frames': sorted(frames.values(), key=lambda f: f['id']),
        'animations': out_anims,
    }
    with open(args.out, 'w', newline='\n') as f:
        json.dump(manifest, f, indent=1)
        f.write('\n')
    print(f"{len(sources)} sources, {len(frames)} frames, {len(out_anims)} animations")


if __name__ == '__main__':
    main()
