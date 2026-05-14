$p = Start-Process -FilePath '.\PeztoldPaint.exe' -PassThru -RedirectStandardOutput 'stdout.txt' -RedirectStandardError 'stderr.txt'
Start-Sleep -Seconds 5
if(-not $p.HasExited) {
    Write-Host 'Process is running - checking for debug output'
    Stop-Process $p.Id -Force
}
Get-Content stdout.txt -ErrorAction SilentlyContinue
Get-Content stderr.txt -ErrorAction SilentlyContinue