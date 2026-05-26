#!/usr/bin/env python3
from PIL import Image
import sys
import os

def usage():
    print("Usage: sprite_sheet_to_header.py input.png output.h [--mono]")

if __name__ == '__main__':
    if len(sys.argv) < 3:
        usage(); sys.exit(1)
    inp = sys.argv[1]
    out = sys.argv[2]
    mono = True

    img = Image.open(inp).convert('L')
    w,h = img.size
    pix = img.load()

    # compute vertical projection to segment sprites
    col_nonzero = [0]*w
    for x in range(w):
        s = 0
        for y in range(h):
            if pix[x,y] < 240:
                s += 1
        col_nonzero[x] = s

    # consider a column blank if very few non-white pixels
    blank_thresh = 1
    min_blank_width = 2

    segments = []
    x = 0
    while x < w:
        # skip blanks
        while x < w and col_nonzero[x] <= blank_thresh:
            x += 1
        if x >= w:
            break
        seg_start = x
        # go until we find a run of blanks of at least min_blank_width
        while x < w:
            if col_nonzero[x] <= blank_thresh:
                # potential blank run
                run = 1
                while x+run < w and col_nonzero[x+run] <= blank_thresh:
                    run += 1
                if run >= min_blank_width:
                    break
                x += run
            else:
                x += 1
        seg_end = x-1
        segments.append((seg_start, seg_end))

    sprites = []
    for (x0,x1) in segments:
        # compute top/bottom bounds
        top = h; bottom = 0
        for x in range(x0, x1+1):
            for y in range(h):
                if pix[x,y] < 250:
                    top = min(top,y); bottom = max(bottom,y)
        if top>bottom:
            continue
        sprites.append((x0,top,x1,bottom))

    # export each sprite as a preview PNG
    frames_dir = os.path.join(os.path.dirname(out), '..', 'pic', 'frames')
    os.makedirs(frames_dir, exist_ok=True)
    for i,s in enumerate(sprites):
        x0,y0,x1,y1 = s
        sw = x1-x0+1; sh = y1-y0+1
        crop = img.crop((x0,y0,x1+1,y1+1)).convert('RGBA')
        # create transparent background where white
        datas = crop.getdata()
        newData = []
        for item in datas:
            # item is (L) converted earlier then to RGBA; keep as gray
            # treat near-white as transparent
            if isinstance(item, int):
                v = item
                if v > 240:
                    newData.append((255,255,255,0))
                else:
                    newData.append((0,0,0,255))
            else:
                # RGBA tuple
                if item[0] > 240:
                    newData.append((255,255,255,0))
                else:
                    newData.append((0,0,0,255))
        crop.putdata(newData)
        fn = os.path.join(frames_dir, 'sprite_%d.png' % i)
        crop.save(fn)
        print('Exported frame:', fn, ' pos=', x0, y0, ' size=', sw, 'x', sh)

    # generate header
    with open(out,'w',encoding='utf-8') as fh:
        fh.write('// Auto-generated from {}\n'.format(os.path.basename(inp)))
        fh.write('#pragma once\n#include <stdint.h>\n\n')
        fh.write('typedef struct { const uint16_t w,h; const uint8_t data[]; } sprite_t;\n\n')
        fh.write('static const uint8_t dino_sprite_count = %d;\n\n' % len(sprites))
        for i,s in enumerate(sprites):
            x0,y0,x1,y1 = s
            sw = x1-x0+1; sh = y1-y0+1
            # pack bits per row (MSB first)
            rows = []
            for y in range(y0,y1+1):
                rowbytes = []
                cur = 0; bits = 0
                for x in range(x0,x1+1):
                    bit = 1 if pix[x,y] < 250 else 0
                    cur = (cur<<1) | bit
                    bits += 1
                    if bits == 8:
                        rowbytes.append(cur)
                        cur = 0; bits = 0
                if bits>0:
                    cur = cur << (8-bits)
                    rowbytes.append(cur)
                rows.append(rowbytes)
            # flatten
            flat = [b for row in rows for b in row]
            fh.write('static const uint8_t sprite_%d_data[] = {'%i)
            fh.write(','.join(str(x) for x in flat))
            fh.write('};\n')
            fh.write('static const uint16_t sprite_%d_w = %d;\n' % (i, sw))
            fh.write('static const uint16_t sprite_%d_h = %d;\n' % (i, sh))
            fh.write('\n')
        # build index
        fh.write('typedef struct { uint16_t w,h; const uint8_t *data; } sprite_index_t;\n')
        fh.write('static const sprite_index_t dino_sprites[] = {\n')
        for i,s in enumerate(sprites):
            fh.write('  { sprite_%d_w, sprite_%d_h, sprite_%d_data },\n' % (i,i,i))
        fh.write('};\n')
    print('Wrote', out)
