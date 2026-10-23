#!/usr/bin/env python3
"""Read a Godot 3 SpriteFrames .tres: textures, atlas regions, animations."""
import re


def parse(path):
    text = open(path, encoding='utf-8').read()
    ext = {int(i): p for p, i in re.findall(
        r'\[ext_resource path="([^"]+)" type="Texture" id=(\d+)\]', text)}
    atlas = {}
    for sid, body in re.findall(
            r'\[sub_resource type="AtlasTexture" id=(\d+)\]\n(.*?)(?=\n\[|\Z)', text, re.S):
        m = re.search(r'atlas = ExtResource\( (\d+) \)', body)
        if not m:  # an imported stream texture outside this sprite set
            continue
        tex = int(m.group(1))
        x, y, w, h = (int(float(v)) for v in re.search(
            r'region = Rect2\( ([-\d.]+), ([-\d.]+), ([-\d.]+), ([-\d.]+) \)', body).groups())
        atlas[int(sid)] = (ext[tex], (x, y, w, h))
    anims = {}
    for body in re.findall(r'\{\n"frames": \[(.*?)\],\n"loop": (true|false),\n"name": "([^"]+)",\n"speed": ([-\d.]+)\n\}', text, re.S):
        frames = [int(v) for v in re.findall(r'SubResource\( (\d+) \)', body[0])]
        if any(f not in atlas for f in frames):
            continue
        anims[body[2]] = {'frames': [atlas[f] for f in frames], 'loop': body[1] == 'true',
                          'speed': float(body[3])}
    return anims


if __name__ == '__main__':
    import sys
    for name, a in sorted(parse(sys.argv[1]).items()):
        files = sorted({f[0].rsplit('/', 1)[-1] for f in a['frames']})
        print(f"{name:14s} n={len(a['frames']):2d} speed={a['speed']:5.1f} loop={a['loop']} {files}")
