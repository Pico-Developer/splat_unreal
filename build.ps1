#
# Copyright (c) 2025 PICO Technology Co., Ltd. See LICENSE.md.
#
# This script builds the plugin as it in the same way that FAB would.
#

param (
    [Parameter(Mandatory=$true)]
    [string]$EngineDir,

    [Parameter(Mandatory=$true)]
    [string]$InPath,

    [Parameter(Mandatory=$true)]
    [string]$OutPath
)

if (!(Test-Path -Path "$InPath")) {
    Write-Host "Directory $InPath doesn't exist. Exiting."
    exit 1
}

if (Test-Path -Path "$OutPath") {
    Write-Host "Directory $OutPath already exists. Exiting."
    exit 1
}

# RunUAT doesn't handle `~` properly, so ensure it's expanded.
# `Resolve-Path` requires the path to be valid, so create `$OutPath`.
$runUat = Resolve-Path "$EngineDir\Engine\Build\BatchFiles\RunUAT.bat"
$plugin = Resolve-Path "$InPath\PICOSplat.uplugin"
New-Item -Path "$OutPath" -ItemType Directory | Out-Null
$package = Resolve-Path "$OutPath"

# From <https://support.fab.com/s/article/FAB-TECHNICAL-REQUIREMENTS>,
# section 4.3.6.2.b (with additional `StrictIncludes`):
& $runUat BuildPlugin -Plugin="$plugin" -Package="$package" -Rocket -StrictIncludes