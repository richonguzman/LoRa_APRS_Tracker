# IceNav Mass Copy Tool - User Manual

A high-performance bash script optimized for copying millions of small map tiles to SD cards using `rsync`, with **anti-fragmentation sorting**.

## Features

- ✅ **Anti-fragmentation** - Sequential Z→X→Y write order for optimal SD card read performance.
- ✅ **Fast Dir Creation** - Parallel directory pre-creation using `xargs`.
- ✅ **Dual Modes** - `incremental` for fast development sync, `full` for production/defragmentation.
- ✅ **Integrity Check** - Random sample verification after sync.
- ✅ **Optimized Mounting** - Uses `noatime`, `nodiratime`, and `async` for maximum write speed.
- ✅ **Clear Logging** - Standardized, color-coded info/warn/error messages.

## Usage

### Basic Syntax
```bash
./rsync_copy.sh [SOURCE] [DESTINATION_MOUNT] [DEVICE] [MODE]
```

- **SOURCE**: Directory containing map tiles (e.g., `./NAVMAP`).
- **DESTINATION_MOUNT**: Where to mount the SD card (e.g., `/mnt/sd`).
- **DEVICE**: SD card device path (e.g., `/dev/sdb1`).
- **MODE**: `incremental` (default) or `full`.

### Examples
```bash
# Quick development sync (updates only)
./rsync_copy.sh ./NAVMAP /mnt/sd /dev/sdc1

# Production deployment (wipes destination, zero fragmentation)
./rsync_copy.sh ./NAVMAP /mnt/sd /dev/sdc1 full
```

---

## SD Card Optimization (Recommended)

For optimal performance with IceNav, format your SD card with **8KB cluster size**. This significantly reduces space waste and improves read latency for small tile files.

### Formatting (Linux)
```bash
# 1. Unmount if mounted
sudo umount /dev/sdX1

# 2. Format with FAT32 and 8KB cluster
sudo mkfs.vfat -F 32 -s 16 -n "ICENAV" /dev/sdX1
# -s 16 = 16 sectors × 512 bytes = 8KB cluster

# 3. Verify
sudo fsck.fat -v /dev/sdX1 | grep "bytes per cluster"
```

---

## Operating Modes

| Mode | Strategy | Best For... |
|------|----------|-------------|
| **Incremental** | Syncs only changed files. | Frequent development updates. |
| **Full** | Wipes destination folder and copies everything in strict order. | Production releases and periodic defragmentation. |

---

## Installation

1. Make the script executable:
   ```bash
   chmod +x rsync_copy.sh
   ```
2. Ensure `rsync` is installed:
   ```bash
   sudo apt install rsync
   ```

## Workflow
1. **Mount**: Device is mounted with high-performance flags.
2. **Verify**: Checks source size vs. destination free space.
3. **Sort**: Generates a file list in Z→X→Y order.
4. **Pre-create**: Rapidly builds folder structure.
5. **Sync**: Executes `rsync` with `--inplace` and `--no-compress`.
6. **Verify**: Compares file counts and checks random samples.
7. **Cleanup**: Safely syncs and unmounts.

---
**Note**: Always use **Full Mode** before long trips to ensure the SD card is perfectly defragmented for the ESP32.