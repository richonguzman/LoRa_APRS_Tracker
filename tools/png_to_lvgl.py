#!/usr/bin/env python3
"""
Convert PNG image to LVGL C array (RGB565 format with alpha)
Usage: python png_to_lvgl.py input.png output.c variable_name
"""

import sys
from PIL import Image

def convert_png_to_lvgl(input_path, output_path, var_name):
    img = Image.open(input_path).convert('RGBA')
    width, height = img.size
    pixels = list(img.getdata())

    # LVGL image header for ARGB8565 (CF_TRUE_COLOR_ALPHA)
    cf = 5  # LV_IMG_CF_TRUE_COLOR_ALPHA

    c_code = f'''// Auto-generated LVGL image: {var_name}
// Size: {width}x{height}

#include "lvgl.h"

#ifndef LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_MEM_ALIGN
#endif

#ifndef LV_ATTRIBUTE_IMG_{var_name.upper()}
#define LV_ATTRIBUTE_IMG_{var_name.upper()}
#endif

const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_IMG_{var_name.upper()} uint8_t {var_name}_map[] = {{
'''

    # Convert pixels to RGB565 + Alpha (3 bytes per pixel)
    data = []
    for r, g, b, a in pixels:
        # RGB565: RRRRRGGG GGGBBBBB
        rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        # Store as little-endian + alpha
        data.append(rgb565 & 0xFF)         # Low byte
        data.append((rgb565 >> 8) & 0xFF)  # High byte
        data.append(a)                      # Alpha

    # Format as C array
    for i, byte in enumerate(data):
        if i % 12 == 0:
            c_code += '\n    '
        c_code += f'0x{byte:02x}, '

    c_code += f'''
}};

const lv_img_dsc_t {var_name} = {{
    .header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA,
    .header.always_zero = 0,
    .header.reserved = 0,
    .header.w = {width},
    .header.h = {height},
    .data_size = {len(data)},
    .data = {var_name}_map,
}};
'''

    with open(output_path, 'w') as f:
        f.write(c_code)

    print(f"Converted {input_path} -> {output_path}")
    print(f"Size: {width}x{height}, {len(data)} bytes")

if __name__ == '__main__':
    if len(sys.argv) != 4:
        print("Usage: python png_to_lvgl.py input.png output.c variable_name")
        sys.exit(1)

    convert_png_to_lvgl(sys.argv[1], sys.argv[2], sys.argv[3])
