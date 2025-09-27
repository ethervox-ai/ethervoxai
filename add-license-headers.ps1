# License Header Addition Script for EthervoxAI
# Adds CC-BY-NC-SA-4.0 license headers to all source files

$cHeaderTemplate = @"
/**
 * @file {filename}
 * @brief {description}
 * 
 * Copyright (c) 2024-2025 EthervoxAI Team
 * 
 * This file is part of EthervoxAI, licensed under CC BY-NC-SA 4.0.
 * You are free to share and adapt this work under the following terms:
 * - Attribution: Credit the original authors
 * - NonCommercial: Not for commercial use
 * - ShareAlike: Distribute under same license
 * 
 * For full license terms, see: https://creativecommons.org/licenses/by-nc-sa/4.0/
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

"@

$jsHeaderTemplate = @"
/**
 * @file {filename}
 * @brief {description}
 * 
 * Copyright (c) 2024-2025 EthervoxAI Team
 * 
 * This file is part of EthervoxAI, licensed under CC BY-NC-SA 4.0.
 * You are free to share and adapt this work under the following terms:
 * - Attribution: Credit the original authors
 * - NonCommercial: Not for commercial use
 * - ShareAlike: Distribute under same license
 * 
 * For full license terms, see: https://creativecommons.org/licenses/by-nc-sa/4.0/
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 */

"@

# File descriptions mapping
$descriptions = @{
    "audio_core.c" = "Core audio processing functionality for EthervoxAI"
    "platform_linux.c" = "Linux-specific audio platform implementation for EthervoxAI"
    "platform_esp32.c" = "ESP32-specific audio platform implementation for EthervoxAI"
    "platform_rpi.c" = "Raspberry Pi-specific audio platform implementation for EthervoxAI"
    "dialogue_core.c" = "Core dialogue processing functionality for EthervoxAI"
    "plugin_manager.c" = "Plugin management system for EthervoxAI"
    "ethervox_sdk.c" = "EthervoxAI SDK implementation"
    "ethervox_sdk.h" = "EthervoxAI SDK header definitions"
    "audio.h" = "Audio processing interface definitions for EthervoxAI"
    "dialogue.h" = "Dialogue processing interface definitions for EthervoxAI"
    "platform.h" = "Platform abstraction layer interface for EthervoxAI"
    "plugins.h" = "Plugin system interface definitions for EthervoxAI"
}

function Add-HeaderToFile {
    param(
        [string]$FilePath,
        [string]$Template,
        [string]$Description
    )
    
    $filename = Split-Path $FilePath -Leaf
    $content = Get-Content $FilePath -Raw
    
    # Skip if file already has license header
    if ($content -match "Copyright.*EthervoxAI Team") {
        Write-Host "Skipping $filename - already has license header"
        return
    }
    
    $header = $Template -replace "\{filename\}", $filename -replace "\{description\}", $Description
    $newContent = $header + $content
    
    Set-Content -Path $FilePath -Value $newContent -NoNewline
    Write-Host "Added license header to $filename"
}

# Process remaining C/C++ files
$cFiles = Get-ChildItem -Path "src", "include", "sdk" -Recurse -Include "*.c", "*.cpp", "*.h", "*.hpp" | Where-Object {
    $_.FullName -notmatch "node_modules" -and
    $_.FullName -notmatch "\.git" -and
    $_.FullName -notmatch "build" -and
    $_.FullName -notmatch "dist" -and
    $(
        $content = Get-Content $_.FullName -Raw -ErrorAction SilentlyContinue
        return $content -and $content -notmatch "Copyright.*EthervoxAI Team"
    )
}

foreach ($file in $cFiles) {
    $filename = $file.Name
    $description = $descriptions[$filename]
    if (-not $description) {
        $description = "Source file for EthervoxAI"
    }
    
    Add-HeaderToFile -FilePath $file.FullName -Template $cHeaderTemplate -Description $description
}

# Process JavaScript/Vue files
$jsFiles = Get-ChildItem -Path "dashboard" -Recurse -Include "*.js", "*.vue", "*.ts" | Where-Object {
    $_.FullName -notmatch "node_modules" -and
    $_.FullName -notmatch "\.git" -and
    $_.FullName -notmatch "build" -and
    $_.FullName -notmatch "dist" -and
    $_.FullName -notmatch "\.nuxt" -and
    $(
        $content = Get-Content $_.FullName -Raw -ErrorAction SilentlyContinue
        return $content -and $content -notmatch "Copyright.*EthervoxAI Team" -and $_.Name -notmatch "config"
    )
}

foreach ($file in $jsFiles) {
    $filename = $file.Name
    $description = "Dashboard component for EthervoxAI"
    
    Add-HeaderToFile -FilePath $file.FullName -Template $jsHeaderTemplate -Description $description
}

Write-Host "License header addition complete!"