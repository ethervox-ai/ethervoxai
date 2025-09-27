# Markdown Formatting Fix Script for EthervoxAI
# Fixes common markdown linting issues across all .md files

$mdFiles = Get-ChildItem -Path "." -Recurse -Include "*.md" | Where-Object {
    $_.FullName -notmatch "node_modules" -and
    $_.FullName -notmatch "\.git" -and
    $_.FullName -notmatch "build" -and
    $_.FullName -notmatch "dist"
}

function Update-MarkdownFile {
    param([string]$FilePath)
    
    $filename = Split-Path $FilePath -Leaf
    Write-Host "Processing $filename..."
    
    $content = Get-Content $FilePath -Raw
    if (-not $content) {
        Write-Host "  Skipping empty file"
        return
    }
    
    # Split into lines for processing
    $lines = $content -split "`r`n|`r|`n"
    $fixedLines = @()
    
    for ($i = 0; $i -lt $lines.Length; $i++) {
        $line = $lines[$i]
        
        # Fix long lines by breaking them at reasonable points
        if ($line.Length -gt 120) {
            # Don't break URLs or code blocks
            if ($line -notmatch "^```.+" -and $line -notmatch "https?://" -and $line -notmatch "^\s*\|") {
                # Try to break at sentence boundaries or commas
                if ($line -match "^(.{1,120}[.!?])\s+(.+)$") {
                    $fixedLines += $matches[1]
                    $line = $matches[2]
                } elseif ($line -match "^(.{1,120},)\s+(.+)$") {
                    $fixedLines += $matches[1]
                    $line = $matches[2]
                } elseif ($line -match "^(.{1,120}\s+)(.+)$") {
                    $fixedLines += $matches[1].TrimEnd()
                    $line = $matches[2]
                }
            }
        }
        
        $fixedLines += $line
        
        # Add blank line after headings if next line isn't blank and isn't another heading
        if ($line -match "^#{1,6}\s+" -and $i -lt $lines.Length - 1) {
            $nextLine = $lines[$i + 1]
            if ($nextLine -ne "" -and $nextLine -notmatch "^#{1,6}\s+" -and $nextLine -notmatch "^\s*$") {
                $fixedLines += ""
            }
        }
    }
    
    # Join the lines back together
    $newContent = $fixedLines -join "`n"
    
    # Only update file if content changed
    if ($newContent -ne $content) {
        Set-Content -Path $FilePath -Value $newContent -NoNewline -Encoding UTF8
        Write-Host "  Fixed formatting issues in $filename"
    } else {
        Write-Host "  No changes needed for $filename"
    }
}

Write-Host "=== Markdown Formatting Fix ==="
Write-Host "Processing $(($mdFiles).Count) markdown files..."
Write-Host ""

foreach ($file in $mdFiles) {
    Update-MarkdownFile -FilePath $file.FullName
}

Write-Host ""
Write-Host "Markdown formatting complete!"