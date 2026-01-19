#!/usr/bin/env python3
"""
OSM Tile Downloader for LoRa APRS Tracker
Downloads map tiles for offline use on T-Deck

Usage:
    python download_tiles.py --region IDF --bbox 48.1,1.4,49.2,3.6 --zoom 10,12,14
    python download_tiles.py --region ARA --bbox 44.5,3.5,46.5,7.2 --zoom 10,12,14

Structure created:
    /output_dir/REGION/zoom/x/y.png
"""

import os
import sys
import math
import time
import argparse
import urllib.request
from pathlib import Path

# OSM tile servers (use only one to respect usage policy)
TILE_SERVERS = [
    "https://tile.openstreetmap.org/{z}/{x}/{y}.png",
    # Alternatives (check usage policies):
    # "https://a.tile.openstreetmap.org/{z}/{x}/{y}.png",
    # "https://b.tile.openstreetmap.org/{z}/{x}/{y}.png",
    # "https://c.tile.openstreetmap.org/{z}/{x}/{y}.png",
]

# User agent (required by OSM usage policy)
USER_AGENT = "LoRa_APRS_Tracker_TileDownloader/1.0"

# Delay between requests (respect OSM usage policy: max 1 req/sec)
REQUEST_DELAY = 1.0


def lat_lon_to_tile(lat, lon, zoom):
    """Convert latitude/longitude to tile coordinates"""
    n = 2 ** zoom
    x = int((lon + 180.0) / 360.0 * n)
    lat_rad = math.radians(lat)
    y = int((1.0 - math.asinh(math.tan(lat_rad)) / math.pi) / 2.0 * n)
    return x, y


def tile_to_lat_lon(x, y, zoom):
    """Convert tile coordinates to latitude/longitude (NW corner)"""
    n = 2 ** zoom
    lon = x / n * 360.0 - 180.0
    lat_rad = math.atan(math.sinh(math.pi * (1 - 2 * y / n)))
    lat = math.degrees(lat_rad)
    return lat, lon


def download_tile(x, y, zoom, output_path):
    """Download a single tile"""
    url = TILE_SERVERS[0].format(z=zoom, x=x, y=y)

    # Create directory if needed
    os.makedirs(os.path.dirname(output_path), exist_ok=True)

    # Skip if already exists
    if os.path.exists(output_path):
        return True, "exists"

    try:
        request = urllib.request.Request(url)
        request.add_header('User-Agent', USER_AGENT)

        with urllib.request.urlopen(request, timeout=30) as response:
            with open(output_path, 'wb') as f:
                f.write(response.read())
        return True, "downloaded"
    except Exception as e:
        return False, str(e)


def get_tiles_for_bbox(min_lat, min_lon, max_lat, max_lon, zoom):
    """Get all tile coordinates for a bounding box at a given zoom level"""
    # Get tile range
    x_min, y_max = lat_lon_to_tile(min_lat, min_lon, zoom)
    x_max, y_min = lat_lon_to_tile(max_lat, max_lon, zoom)

    tiles = []
    for x in range(x_min, x_max + 1):
        for y in range(y_min, y_max + 1):
            tiles.append((x, y))

    return tiles


def estimate_size(tile_count, avg_tile_size_kb=30):
    """Estimate total download size"""
    size_mb = tile_count * avg_tile_size_kb / 1024
    return size_mb


def main():
    parser = argparse.ArgumentParser(description='Download OSM tiles for offline use')
    parser.add_argument('--region', required=True, help='Region name (e.g., IDF, ARA, PACA)')
    parser.add_argument('--bbox', required=True, help='Bounding box: min_lat,min_lon,max_lat,max_lon')
    parser.add_argument('--zoom', default='10,12,14', help='Zoom levels (comma-separated)')
    parser.add_argument('--output', default='./tiles', help='Output directory')
    parser.add_argument('--dry-run', action='store_true', help='Show what would be downloaded')

    args = parser.parse_args()

    # Parse bounding box
    try:
        bbox = [float(x.strip()) for x in args.bbox.split(',')]
        if len(bbox) != 4:
            raise ValueError("Need 4 values")
        min_lat, min_lon, max_lat, max_lon = bbox
    except:
        print("Error: bbox must be min_lat,min_lon,max_lat,max_lon")
        sys.exit(1)

    # Parse zoom levels
    try:
        zoom_levels = [int(z.strip()) for z in args.zoom.split(',')]
    except:
        print("Error: zoom must be comma-separated integers")
        sys.exit(1)

    # Calculate total tiles
    total_tiles = 0
    tiles_by_zoom = {}

    print(f"\n=== OSM Tile Downloader ===")
    print(f"Region: {args.region}")
    print(f"Bounding box: {min_lat:.4f}, {min_lon:.4f} to {max_lat:.4f}, {max_lon:.4f}")
    print(f"Zoom levels: {zoom_levels}")
    print()

    for zoom in zoom_levels:
        tiles = get_tiles_for_bbox(min_lat, min_lon, max_lat, max_lon, zoom)
        tiles_by_zoom[zoom] = tiles
        total_tiles += len(tiles)
        print(f"  Zoom {zoom:2d}: {len(tiles):6d} tiles")

    est_size = estimate_size(total_tiles)
    print(f"\n  Total: {total_tiles} tiles (~{est_size:.1f} MB)")
    print(f"  Estimated time: ~{total_tiles * REQUEST_DELAY / 60:.1f} minutes")

    if args.dry_run:
        print("\n[Dry run - no files downloaded]")
        return

    # Confirm
    print()
    response = input("Proceed with download? [y/N] ")
    if response.lower() != 'y':
        print("Aborted.")
        return

    # Download tiles
    output_base = Path(args.output) / args.region
    downloaded = 0
    skipped = 0
    errors = 0

    print(f"\nDownloading to: {output_base}")
    print()

    for zoom in zoom_levels:
        tiles = tiles_by_zoom[zoom]
        print(f"Zoom {zoom}:")

        for i, (x, y) in enumerate(tiles):
            output_path = output_base / str(zoom) / str(x) / f"{y}.png"

            success, status = download_tile(x, y, zoom, output_path)

            if success:
                if status == "exists":
                    skipped += 1
                    marker = "."
                else:
                    downloaded += 1
                    marker = "+"
                    time.sleep(REQUEST_DELAY)  # Respect rate limit
            else:
                errors += 1
                marker = "X"
                print(f"\n  Error downloading {zoom}/{x}/{y}: {status}")

            # Progress indicator
            if (i + 1) % 50 == 0:
                print(f"  [{i+1}/{len(tiles)}]")
            else:
                print(marker, end='', flush=True)

        print()

    print(f"\n=== Complete ===")
    print(f"Downloaded: {downloaded}")
    print(f"Skipped (existing): {skipped}")
    print(f"Errors: {errors}")
    print(f"\nCopy '{output_base}' to SD card: /LoRa_Tracker/Maps/{args.region}/")


# Predefined regions (France)
REGIONS = {
    'IDF': {
        'name': 'Île-de-France',
        'bbox': (48.1, 1.4, 49.2, 3.6)
    },
    'ARA': {
        'name': 'Auvergne-Rhône-Alpes',
        'bbox': (44.1, 2.0, 46.8, 7.2)
    },
    'PACA': {
        'name': "Provence-Alpes-Côte d'Azur",
        'bbox': (42.9, 4.2, 45.1, 7.7)
    },
    'OCC': {
        'name': 'Occitanie',
        'bbox': (42.3, -0.4, 45.0, 4.9)
    },
    'NAQ': {
        'name': 'Nouvelle-Aquitaine',
        'bbox': (44.0, -1.8, 47.2, 2.6)
    },
    'BRE': {
        'name': 'Bretagne',
        'bbox': (47.2, -5.2, 48.9, -0.9)
    },
    'NOR': {
        'name': 'Normandie',
        'bbox': (48.2, -2.0, 49.8, 1.8)
    },
    'HDF': {
        'name': 'Hauts-de-France',
        'bbox': (48.8, 1.4, 51.1, 4.3)
    },
    'GES': {
        'name': 'Grand Est',
        'bbox': (47.4, 3.4, 49.6, 8.3)
    },
    'BFC': {
        'name': 'Bourgogne-Franche-Comté',
        'bbox': (46.1, 2.8, 48.4, 7.2)
    },
    'PDL': {
        'name': 'Pays de la Loire',
        'bbox': (46.2, -2.6, 48.6, 0.9)
    },
    'CVL': {
        'name': 'Centre-Val de Loire',
        'bbox': (46.3, 0.0, 48.6, 3.2)
    },
    'COR': {
        'name': 'Corse',
        'bbox': (41.3, 8.5, 43.1, 9.6)
    },
}


def list_regions():
    """Show predefined regions"""
    print("\n=== Predefined Regions (France) ===\n")
    for code, info in REGIONS.items():
        bbox = info['bbox']
        print(f"  {code:4s} - {info['name']}")
        print(f"         --bbox {bbox[0]},{bbox[1]},{bbox[2]},{bbox[3]}")
        print()


if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == '--list':
        list_regions()
    else:
        main()
