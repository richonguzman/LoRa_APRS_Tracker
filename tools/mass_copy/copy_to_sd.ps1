<#
.SYNOPSIS
    Fast mass copy with anti-fragmentation for IceNav tiles (Windows).

.DESCRIPTION
    Copies NAV tiles to SD card with sequential write order (Z/X/Y sorted)
    to minimize FAT32 fragmentation. Equivalent of rsync_copy.sh for Windows.

.PARAMETER Source
    Directory containing map tiles.

.PARAMETER Destination
    SD card drive letter (e.g. E:).

.PARAMETER DestSubdir
    Subdirectory on SD card (e.g. LoRa_Tracker\VectMaps).
    Defaults to the source directory name.

.PARAMETER Mode
    'incremental' (default) - copy only changed/new files.
    'full' - wipe destination and sequential copy (zero fragmentation).

.EXAMPLE
    .\copy_to_sd.ps1 -Source .\output -Destination E: -DestSubdir LoRa_Tracker\VectMaps
    .\copy_to_sd.ps1 -Source .\output -Destination E: -DestSubdir LoRa_Tracker\VectMaps -Mode full
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$Source,

    [Parameter(Mandatory)]
    [string]$Destination,

    [string]$DestSubdir,

    [ValidateSet('incremental', 'full')]
    [string]$Mode = 'incremental'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# --- Utility functions ---

function Log   { param([string]$Msg) Write-Host "[INFO] " -ForegroundColor Blue -NoNewline; Write-Host $Msg }
function Warn  { param([string]$Msg) Write-Host "[WARN] " -ForegroundColor Yellow -NoNewline; Write-Host $Msg }
function Err   { param([string]$Msg) Write-Host "[ERROR] " -ForegroundColor Red -NoNewline; Write-Host $Msg; exit 1 }
function Ok    { param([string]$Msg) Write-Host "[OK] " -ForegroundColor Green -NoNewline; Write-Host $Msg }

function Format-Bytes {
    param([long]$Bytes)
    if ($Bytes -ge 1GB) { '{0:N2} GB' -f ($Bytes / 1GB) }
    elseif ($Bytes -ge 1MB) { '{0:N2} MB' -f ($Bytes / 1MB) }
    else { '{0:N2} KB' -f ($Bytes / 1KB) }
}

function Get-DirSize {
    param([string]$Path)
    (Get-ChildItem -Path $Path -Recurse -File -ErrorAction SilentlyContinue |
        Measure-Object -Property Length -Sum).Sum
}

# --- Validate inputs ---

$Source = (Resolve-Path $Source -ErrorAction Stop).Path

if (-not (Test-Path $Source -PathType Container)) {
    Err "Source directory '$Source' not found"
}

$driveLetter = $Destination.TrimEnd(':\')
$driveInfo = [System.IO.DriveInfo]::new($driveLetter)
if (-not $driveInfo.IsReady) {
    Err "Drive $Destination is not ready (no SD card?)"
}

if (-not $DestSubdir) {
    $DestSubdir = Split-Path $Source -Leaf
}

$DestPath = Join-Path $Destination $DestSubdir

# --- Banner ---

if ($Mode -eq 'full') {
    Write-Host "`n" -NoNewline
    Write-Host ('{0}{1}{2}' -f [char]0x2554, ([string][char]0x2550 * 49), [char]0x2557) -ForegroundColor Magenta
    Write-Host ('{0}   FULL MODE: Zero Fragmentation Guaranteed     {1}' -f [char]0x2551, [char]0x2551) -ForegroundColor Magenta
    Write-Host ('{0}{1}{2}' -f [char]0x255A, ([string][char]0x2550 * 49), [char]0x255D) -ForegroundColor Magenta
} else {
    Write-Host "`n" -NoNewline
    Write-Host ('{0}{1}{2}' -f [char]0x2554, ([string][char]0x2550 * 49), [char]0x2557) -ForegroundColor Green
    Write-Host ('{0}  INCREMENTAL MODE: Fast Development Sync       {1}' -f [char]0x2551, [char]0x2551) -ForegroundColor Green
    Write-Host ('{0}{1}{2}' -f [char]0x255A, ([string][char]0x2550 * 49), [char]0x255D) -ForegroundColor Green
}

Write-Host "Start: $(Get-Date)" -ForegroundColor Blue
Write-Host ""
Write-Host "Configuration:"
Write-Host "  Source:      $Source"
Write-Host "  Destination: $DestPath\"
Write-Host "  Drive:       $Destination ($($driveInfo.DriveFormat))"
Write-Host "  Mode:        $Mode"
if ($Mode -eq 'full') {
    Write-Host "  Strategy:    Delete + Sequential copy (optimal performance)" -ForegroundColor Magenta
} else {
    Write-Host "  Strategy:    Incremental sync (may fragment over time)" -ForegroundColor Yellow
}
Write-Host ""

# --- Step 1: Space verification ---

$sourceSize = Get-DirSize $Source
$destFree = $driveInfo.AvailableFreeSpace
Log "Source size: $(Format-Bytes $sourceSize) | Free space: $(Format-Bytes $destFree)"

if ($sourceSize -gt $destFree) {
    Err "Insufficient space on $Destination (need $(Format-Bytes $sourceSize), have $(Format-Bytes $destFree))"
}

# --- Step 2: Full mode - wipe existing destination ---

if ($Mode -eq 'full' -and (Test-Path $DestPath)) {
    Log "FULL MODE: Removing existing destination..."
    Remove-Item -Path $DestPath -Recurse -Force
}

# --- Step 3: Build sorted file list (Z/X/Y) ---

Log "Scanning source and sorting files (Z/X/Y)..."

$allFiles = Get-ChildItem -Path $Source -Recurse -File |
    ForEach-Object {
        $rel = $_.FullName.Substring($Source.Length).TrimStart('\', '/')
        $parts = $rel -split '[/\\]'
        # Pad numeric parts for correct sorting (Z/X/Y.ext)
        $sortKey = ($parts | ForEach-Object {
            if ($_ -match '^\d+') { $_.PadLeft(10, '0') } else { $_ }
        }) -join '/'
        [PSCustomObject]@{
            FullName    = $_.FullName
            RelPath     = $rel
            SortKey     = $sortKey
            Length      = $_.Length
            LastWrite   = $_.LastWriteTimeUtc
        }
    } |
    Sort-Object SortKey

$totalFiles = $allFiles.Count
if ($totalFiles -eq 0) {
    Err "No files found in $Source"
}
Log "Files to process: $totalFiles"

# --- Step 4: Pre-create directory structure ---

Log "Pre-creating directory structure..."
$allFiles | ForEach-Object {
    $dir = Join-Path $DestPath (Split-Path $_.RelPath -Parent)
    if ($dir -and -not (Test-Path $dir)) {
        New-Item -Path $dir -ItemType Directory -Force | Out-Null
    }
}

# --- Step 5: Transfer ---

Log "Starting transfer..."
$sw = [System.Diagnostics.Stopwatch]::StartNew()
$copied = 0
$skipped = 0
$bytesCopied = 0

for ($i = 0; $i -lt $totalFiles; $i++) {
    $f = $allFiles[$i]
    $destFile = Join-Path $DestPath $f.RelPath

    $needCopy = $true
    if ($Mode -eq 'incremental' -and (Test-Path $destFile)) {
        $existing = Get-Item $destFile
        if ($existing.Length -eq $f.Length -and $existing.LastWriteTimeUtc -eq $f.LastWrite) {
            $needCopy = $false
            $skipped++
        }
    }

    if ($needCopy) {
        Copy-Item -Path $f.FullName -Destination $destFile -Force
        $copied++
        $bytesCopied += $f.Length
    }

    if (($i % 100) -eq 0 -or $i -eq ($totalFiles - 1)) {
        $pct = [int](($i + 1) / $totalFiles * 100)
        Write-Progress -Activity "Copying tiles" `
            -Status "$($i+1)/$totalFiles ($pct%) - Copied: $copied, Skipped: $skipped" `
            -PercentComplete $pct
    }
}

Write-Progress -Activity "Copying tiles" -Completed
$sw.Stop()

# --- Step 6: Verification ---

$destFileCount = (Get-ChildItem -Path $DestPath -Recurse -File | Measure-Object).Count
Log "Verification: Source $totalFiles | Destination $destFileCount"
if ($totalFiles -eq $destFileCount) {
    Ok "Transfer completed in $([int]$sw.Elapsed.TotalSeconds)s (copied: $copied, skipped: $skipped)"
} else {
    Warn "File count mismatch! Source: $totalFiles, Destination: $destFileCount"
}

# --- Step 7: Sample integrity check ---

Log "Verifying random samples..."
$sampleSize = [Math]::Min(10, $totalFiles)
$samples = $allFiles | Get-Random -Count $sampleSize
$sampleErrors = 0

foreach ($s in $samples) {
    $destFile = Join-Path $DestPath $s.RelPath
    if (-not (Test-Path $destFile)) {
        Write-Host "  " -NoNewline; Write-Host "X" -ForegroundColor Red -NoNewline; Write-Host " $($s.RelPath) (Missing at destination)"
        $sampleErrors++
        continue
    }
    $srcHash = (Get-FileHash -Path $s.FullName -Algorithm MD5).Hash
    $dstHash = (Get-FileHash -Path $destFile -Algorithm MD5).Hash
    if ($srcHash -eq $dstHash) {
        Write-Host "  " -NoNewline; Write-Host "V" -ForegroundColor Green -NoNewline; Write-Host " $($s.RelPath)"
    } else {
        Write-Host "  " -NoNewline; Write-Host "X" -ForegroundColor Red -NoNewline; Write-Host " $($s.RelPath) (Content mismatch)"
        $sampleErrors++
    }
}

if ($sampleErrors -gt 0) {
    Warn "$sampleErrors/$sampleSize samples failed verification!"
} else {
    Ok "All $sampleSize samples verified OK"
}

# --- Summary ---

$totalSeconds = [Math]::Max(1, [int]$sw.Elapsed.TotalSeconds)
$avgSpeed = [int]($bytesCopied / 1MB / $totalSeconds)

Write-Host ""
Log "-------------------------------------------"
Log "  Sync Summary"
Log "-------------------------------------------"
Log "  Files:      $totalFiles (copied: $copied, skipped: $skipped)"
Log "  Size:       $(Format-Bytes $bytesCopied) copied"
Log "  Time:       ${totalSeconds}s"
Log "  Speed:      ${avgSpeed} MB/s"
Log "-------------------------------------------"

if ($Mode -eq 'full') {
    Ok "FULL SYNC COMPLETED - ZERO FRAGMENTATION"
} else {
    Ok "INCREMENTAL SYNC COMPLETED"
}
