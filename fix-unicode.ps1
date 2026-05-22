# Replace Unicode box-drawing and special characters with ASCII equivalents
$replacements = @{
    '✓' = '[OK]'
    '✗' = '[FAIL]'
    '━' = '='
    '┃' = '|'
    '│' = '|'
    '─' = '-'
    '┏' = '+'
    '┓' = '+'
    '┗' = '+'
    '┛' = '+'
    '├' = '+'
    '┤' = '+'
    '┬' = '+'
    '┴' = '+'
    '┼' = '+'
    '╔' = '+'
    '╗' = '+'
    '╚' = '+'
    '╝' = '+'
    '║' = '|'
    '═' = '='
    '╠' = '+'
    '╣' = '+'
    '╦' = '+'
    '╩' = '+'
    '╬' = '+'
    '┣' = '+'
    '┫' = '+'
    '⬇️' = '[downloading]'
}

$fileCount = 0
Get-ChildItem -Recurse -Include *.c,*.h,*.cmake,*.sh,*.md | Where-Object {
    $_.FullName -notmatch '\\external\\' -and 
    $_.FullName -notmatch '\\build\\' -and 
    $_.FullName -notmatch '\\node_modules\\'
} | ForEach-Object {
    $content = Get-Content $_.FullName -Raw -Encoding UTF8
    $modified = $content
    
    foreach ($pair in $replacements.GetEnumerator()) {
        $modified = $modified -replace [regex]::Escape($pair.Key), $pair.Value
    }
    
    if ($modified -ne $content) {
        Set-Content -Path $_.FullName -Value $modified -Encoding UTF8 -NoNewline
        Write-Host "Updated: $($_.FullName)"
        $fileCount++
    }
}

Write-Host "`nTotal files updated: $fileCount"
