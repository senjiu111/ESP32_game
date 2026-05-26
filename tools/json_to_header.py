#!/usr/bin/env python3
import json, os

root = os.path.join(os.path.dirname(__file__), '..')
pic_dir = os.path.join(root, 'pic')
json_path = os.path.join(pic_dir, 'sprites_meta.json')
include_dir = os.path.join(root, 'include')
os.makedirs(include_dir, exist_ok=True)
header_path = os.path.join(include_dir, 'sprites_meta.h')

with open(json_path, 'r', encoding='utf-8') as fh:
    data = json.load(fh)

sprites = data['sprites']
changed = {}
custom_dino_collision = {
    'dino_01.png': {'x': 11, 'y': 25, 'w': 22, 'h': 20},
    'dino_02.png': {'x': 11, 'y': 25, 'w': 22, 'h': 20},
    'dino_03.png': {'x': 11, 'y': 25, 'w': 22, 'h': 20},
    'dino_04.png': {'x': 11, 'y': 25, 'w': 22, 'h': 20},
    'dino_05.png': {'x': 11, 'y': 14, 'w': 32, 'h': 12},
    'dino_06.png': {'x': 11, 'y': 14, 'w': 32, 'h': 12},
}
for name, meta in sprites.items():
    if name in custom_dino_collision:
        col = meta['collision']
        old_x, old_y, old_w, old_h = col['x'], col['y'], col['w'], col['h']
        tuned = custom_dino_collision[name]
        new_x, new_y, new_w, new_h = tuned['x'], tuned['y'], tuned['w'], tuned['h']
        # update
        sprites[name]['collision'] = {'x': new_x, 'y': new_y, 'w': new_w, 'h': new_h}
        # recompute offset from anchor
        anchor = sprites[name]['anchor']
        offset_x = new_x - anchor['x']
        offset_y = new_y - anchor['y']
        sprites[name]['collision_offset_from_anchor'] = {'x': offset_x, 'y': offset_y}
        changed[name] = {'old': {'x':old_x,'y':old_y,'w':old_w,'h':old_h}, 'new': {'x':new_x,'y':new_y,'w':new_w,'h':new_h}}

# write back JSON
with open(json_path, 'w', encoding='utf-8') as fh:
    json.dump(data, fh, ensure_ascii=False, indent=2)

# generate C header
with open(header_path, 'w', encoding='utf-8') as fh:
    fh.write('#pragma once\n#include <stdint.h>\n\n')
    fh.write('typedef struct { const char *name; uint16_t w,h; int16_t anchor_x, anchor_y; uint16_t bbox_x,bbox_y,bbox_w,bbox_h; uint16_t col_x,col_y,col_w,col_h; int16_t col_offset_x, col_offset_y; } SpriteMeta;\n\n')
    fh.write('static const SpriteMeta sprites_meta[] = {\n')
    for name, meta in sprites.items():
        fh.write('  { "%s", %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d },\n' % (
            name, meta['w'], meta['h'], meta['anchor']['x'], meta['anchor']['y'], meta['bbox']['left'], meta['bbox']['top'], meta['bbox']['right'] - meta['bbox']['left'], meta['bbox']['bottom'] - meta['bbox']['top'], meta['collision']['x'], meta['collision']['y'], meta['collision']['w'], meta['collision']['h'], meta['collision_offset_from_anchor']['x'], meta['collision_offset_from_anchor']['y']
        ))
    fh.write('};\n\n')
    fh.write('static const uint16_t sprites_meta_count = %d;\n' % len(sprites))

print('Wrote', json_path)
print('Wrote', header_path)
if changed:
    print('Adjusted collisions for:')
    for k,v in changed.items():
        print(' ', k, '->', v)
else:
    print('No dino collisions adjusted.')
