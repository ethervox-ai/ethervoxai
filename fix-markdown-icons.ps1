# Markdown Icon and Code Block Fix Script for EthervoxAI
# Fixes emoji corruption and missing newlines before code blocks

$mdFiles = Get-ChildItem -Path "." -Recurse -Include "*.md" | Where-Object {
    $_.FullName -notmatch "node_modules" -and
    $_.FullName -notmatch "\.git" -and
    $_.FullName -notmatch "build" -and
    $_.FullName -notmatch "dist"
}

# Emoji mapping to restore corrupted icons
$emojiMap = @{
    'ðŸ¤' = '🤝'
    'ðŸŽ¯' = '🎯'
    'ðŸ›' = '🐛'
    'ðŸ'¡' = '💡'
    'ðŸ"§' = '🔧'
    'ðŸŒ' = '🌍'
    'ðŸ›ï¸' = '🛠️'
    'ðŸ' = '📜'
    'ðŸŒŸ' = '🌟'
    'ðŸ"'' = '🔒'
    'âœ…' = '✅'
    'â' = '❌'
    'ðŸ"Š' = '📊'
    'ðŸ›¡ï¸' = '🛡️'
    'ðŸŽ–ï¸' = '🎖️'
    'ðŸ"' = '📍'
    'ðŸšš' = '🚨'
    'ðŸ"ž' = '📞'
    'ðŸ"‹' = '📋'
    'âš–ï¸' = '⚖️'
    'ðŸŸ¡' = '🟡'
    'ðŸŸ ' = '🟠'
    'ðŸ"´' = '🔴'
    'ðŸ"„' = '🔄'
    'ðŸŒ' = '🌍'
    'ðŸ"š' = '📚'
    'ðŸŽ"' = '🎓'
    'ðŸ†˜' = '🆘'
    'ðŸ"„' = '📄'
    'ðŸšƒ' = '🚀'
    'ðŸŽ™ï¸' = '🎙️'
    'âœ¨' = '✨'
    'ðŸ"¥' = '🔥'
    'ðŸ"Œ' = '🔌'
    'ðŸ"±' = '📱'
    'ðŸ'»' = '💻'
    'â°' = '⚡'
    'ðŸ"' = '🔍'
    'ðŸ"ˆ' = '📈'
    'ðŸ'¬' = '💬'
    'ðŸŽ¤' = '🎤'
    'ðŸ"¦' = '📦'
    'ðŸ"§' = '📧'
    'ðŸŒ' = '🌐'
    'ðŸ"…' = '📅'
    'ðŸ'¥' = '👥'
    'ðŸ"' = '📝'
    'ðŸ"Ž' = '📎'
    'ðŸ"¼' = '📼'
    'ðŸ'¼' = '💼'
    'ðŸŽ­' = '🎭'
    'â€"' = '—'
    'â€™' = '''
    'â€œ' = '"'
    'â€' = '"'
}

function Repair-MarkdownFile {
    param([string]$FilePath)
    
    $filename = Split-Path $FilePath -Leaf
    Write-Host "Repairing $filename..."
    
    $content = Get-Content $FilePath -Raw -Encoding UTF8
    if (-not $content) {
        Write-Host "  Skipping empty file"
        return
    }
    
    $originalContent = $content
    
    # Fix emoji corruption
    foreach ($corrupted in $emojiMap.Keys) {
        $correct = $emojiMap[$corrupted]
        $content = $content -replace [regex]::Escape($corrupted), $correct
    }
    
    # Split into lines for processing
    $lines = $content -split "`r`n|`r|`n"
    $fixedLines = @()
    
    for ($i = 0; $i -lt $lines.Length; $i++) {
        $currentLine = $lines[$i]
        
        # Add blank line before code blocks if missing
        if ($currentLine -match "^```" -and $i -gt 0) {
            $previousLine = $lines[$i - 1]
            if ($previousLine -ne "" -and $previousLine -notmatch "^\s*$") {
                # Previous line is not blank, add blank line before code block
                if ($fixedLines.Count -gt 0 -and $fixedLines[-1] -ne "") {
                    $fixedLines += ""
                }
            }
        }
        
        $fixedLines += $currentLine
    }
    
    # Join the lines back together
    $newContent = $fixedLines -join "`n"
    
    # Only update file if content changed
    if ($newContent -ne $originalContent) {
        Set-Content -Path $FilePath -Value $newContent -NoNewline -Encoding UTF8
        Write-Host "  Fixed icons and code blocks in $filename"
    } else {
        Write-Host "  No changes needed for $filename"
    }
}

Write-Host "=== Markdown Icon and Code Block Repair ==="
Write-Host "Processing $(($mdFiles).Count) markdown files..."
Write-Host ""

foreach ($file in $mdFiles) {
    Repair-MarkdownFile -FilePath $file.FullName
}

Write-Host ""
Write-Host "Markdown repair complete!"