#!/usr/bin/env python3
"""
Convert PNG map tiles to RGB565 raw format for faster loading on ESP32.
Raw files are ~3-6x bigger but load much faster (no PNG decoding).
"""

import os
import sys
from PIL import Image
from pathlib import Path
import argparse

def convert_png_to_raw(png_path, keep_png=True):
    """Convert a PNG tile to RGB565 raw format."""
    try:
        img = Image.open(png_path).convert("RGB")

        if img.size != (256, 256):
            print(f"  Warning: {png_path} is {img.size}, expected 256x256")

        raw_path = str(png_path).replace(".png", ".raw")

        # Convert to RGB565 (little endian)
        data = bytearray()
        for y in range(img.height):
            for x in range(img.width):
                r, g, b = img.getpixel((x, y))
                # RGB565: RRRRRGGGGGGBBBBB
                rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
                data.extend(rgb565.to_bytes(2, 'little'))

        with open(raw_path, "wb") as f:
            f.write(data)

        if not keep_png:
            os.remove(png_path)

        return True
    except Exception as e:
        print(f"  Error converting {png_path}: {e}")
        return False

def convert_directory(base_path, keep_png=True):
    """Convert all PNG tiles in a directory tree."""
    base = Path(base_path)

    if not base.exists():
        print(f"Error: {base_path} does not exist")
        return

    # Find all PNG files
    png_files = list(base.rglob("*.png"))
    total = len(png_files)

    if total == 0:
        print(f"No PNG files found in {base_path}")
        return

    print(f"Found {total} PNG tiles to convert")
    print(f"Keep PNG files: {keep_png}")
    print()

    converted = 0
    errors = 0

    for i, png_path in enumerate(png_files):
        if (i + 1) % 100 == 0 or i == 0:
            print(f"Converting {i+1}/{total} ({100*(i+1)//total}%)...")

        if convert_png_to_raw(png_path, keep_png):
            converted += 1
        else:
            errors += 1

    print()
    print(f"Done! Converted: {converted}, Errors: {errors}")

    # Show space comparison
    png_size = sum(f.stat().st_size for f in base.rglob("*.png"))
    raw_size = sum(f.stat().st_size for f in base.rglob("*.raw"))
    print(f"PNG total: {png_size / 1024 / 1024:.1f} MB")
    print(f"RAW total: {raw_size / 1024 / 1024:.1f} MB")
    print(f"Ratio: {raw_size / png_size:.1f}x")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Convert PNG map tiles to RGB565 raw format")
    parser.add_argument("path", help="Path to atlas directory containing PNG tiles")
    parser.add_argument("--delete-png", action="store_true", help="Delete PNG files after conversion")

    args = parser.parse_args()

    convert_directory(args.path, keep_png=not args.delete_png)
