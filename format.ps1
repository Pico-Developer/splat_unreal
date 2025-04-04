#
# Copyright (c) 2025 PICO Technology Co., Ltd. See LICENSE.md.
#
# Formats all source files via clang-format, skipping symlinks.
#

$pluginDir = $PSScriptRoot

# Collect all C++ and shader files.
$files = Get-ChildItem -Path $pluginDir -Recurse -Include *.h, *.cpp, *.hlsl, *.ush, *.usf

# Exclude symlinked shaders, as their target shaders will already be picked up.
$files | Where-Object { $_.LinkType -eq $null } | ForEach-Object { clang-format -i $_.FullName }
