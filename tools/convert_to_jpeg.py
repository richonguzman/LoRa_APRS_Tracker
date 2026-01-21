#!/usr/bin/env python3
"""
Convert PNG map tiles to JPEG format for smaller file size.
JPEG tiles are typically 3-5x smaller than PNG with minimal quality loss.
"""

import os
import sys
from PIL import Image
from pathlib import Path
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed

def convert_png_to_jpeg(png_path, quality=85, keep_png=False):
    """Convert a PNG tile to JPEG format."""
    try:
        img = Image.open(png_path).convert("RGB")

        jpeg_path = str(png_path).replace(".png", ".jpg")

        img.save(jpeg_path, "JPEG", quality=quality, optimize=True)

        if not keep_png:
            os.remove(png_path)

        return True, png_path
    except Exception as e:
        return False, f"{png_path}: {e}"

def convert_directory(base_path, quality=85, keep_png=False, jobs=4):
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
    print(f"JPEG quality: {quality}")
    print(f"Keep PNG files: {keep_png}")
    print(f"Parallel jobs: {jobs}")
    print()

    converted = 0
    errors = 0

    with ThreadPoolExecutor(max_workers=jobs) as executor:
        futures = {
            executor.submit(convert_png_to_jpeg, png_path, quality, keep_png): png_path
            for png_path in png_files
        }

        for i, future in enumerate(as_completed(futures)):
            success, result = future.result()
            if success:
                converted += 1
            else:
                errors += 1
                print(f"  Error: {result}")

            if (i + 1) % 500 == 0 or i == 0 or i == total - 1:
                print(f"Progress: {i+1}/{total} ({100*(i+1)//total}%)")

    print()
    print(f"Done! Converted: {converted}, Errors: {errors}")

    # Show space comparison
    if keep_png:
        png_size = sum(f.stat().st_size for f in base.rglob("*.png"))
        print(f"PNG total: {png_size / 1024 / 1024:.1f} MB")

    jpeg_size = sum(f.stat().st_size for f in base.rglob("*.jpg"))
    print(f"JPEG total: {jpeg_size / 1024 / 1024:.1f} MB")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Convert PNG map tiles to JPEG format")
    parser.add_argument("path", help="Path to directory containing PNG tiles")
    parser.add_argument("-q", "--quality", type=int, default=85,
                        help="JPEG quality (1-100, default: 85)")
    parser.add_argument("-j", "--jobs", type=int, default=4,
                        help="Number of parallel conversion jobs (default: 4)")
    parser.add_argument("--keep-png", action="store_true",
                        help="Keep PNG files after conversion")

    args = parser.parse_args()

    convert_directory(args.path, args.quality, args.keep_png, args.jobs)
