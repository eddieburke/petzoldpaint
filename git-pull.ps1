$result = git pull
if ($LASTEXITCODE -eq 0) {
    Write-Host "[OK] Pulled latest"
} else {
    Write-Host "[FAILED] Pull failed with exit code $LASTEXITCODE"
}
