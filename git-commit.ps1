param([string]$Message)

if (-not $Message) {
    $Message = Read-Host 'Commit message'
}

$result = git add -A
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$result = git commit -m $Message
if ($LASTEXITCODE -eq 0) {
    Write-Host "[OK] Committed"
} else {
    Write-Host "[FAILED] Commit failed with exit code $LASTEXITCODE"
}
