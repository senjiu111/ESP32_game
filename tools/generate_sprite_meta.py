#!/usr/bin/env python3
from PIL import Image
import os, json

pic_dir = os.path.join(os.path.dirname(__file__), '..', 'pic')
out_path = os.path.join(pic_dir, 'sprites_meta.json')
files = sorted([f for f in os.listdir(pic_dir) if f.lower().endswith('.png')])
meta = {}
for fn in files:
    path = os.path.join(pic_dir, fn)
    im = Image.open(path).convert('RGBA')
    w,h = im.size
    # compute bbox of visible near-nonwhite pixels; transparent pixels are background
    mask = Image.new('L', (w, h), 0)
    src = im.load()
    dst = mask.load()
    for y in range(h):
        for x in range(w):
            r, g, b, a = src[x, y]
            luminance = (r * 299 + g * 587 + b * 114) // 1000
            if a >= 16 and luminance < 241:
                dst[x, y] = 255
    bbox = mask.getbbox()  # (left, top, right, bottom)
    if bbox:
        left, top, right, bottom = bbox
        bw = right-left
        bh = bottom-top
    else:
        left = top = 0; bw = 0; bh = 0
    anchor = {'x': w//2, 'y': h-1}  # default: bottom-center
    # collision box: use bbox; if empty, use full image height  (transparent fallback)
    collision = {'x': left, 'y': top, 'w': bw, 'h': bh}
    # offset from anchor (collision box top-left relative to anchor)
    offset = {'x': left - anchor['x'], 'y': top - anchor['y']}
    meta[fn] = {
        'w': w, 'h': h,
        'bbox': {'left': left, 'top': top, 'right': left + bw, 'bottom': top + bh},
        'anchor': anchor,
        'collision': collision,
        'collision_offset_from_anchor': offset,
        'notes': ''
    }

with open(out_path, 'w', encoding='utf-8') as fh:
    json.dump({'generated_from': files, 'sprites': meta}, fh, ensure_ascii=False, indent=2)

print('Wrote', out_path)
for k,v in meta.items():
    print(k, 'size=%dx%d' % (v['w'], v['h']), 'bbox=', v['bbox'], 'anchor=', v['anchor'], 'collision=', v['collision'])
