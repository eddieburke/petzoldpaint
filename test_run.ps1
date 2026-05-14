$p = Start-Process -FilePath '.\PeztoldPaint.exe' -PassThru
Start-Sleep -Seconds 3
if(-not $p.HasExited) {
    Write-Host 'Running'
    Stop-Process $p.Id -Force
} else {
    Write-Host ('Exited: ' + $p.ExitCode)
}