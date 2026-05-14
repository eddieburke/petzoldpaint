Add-Type -AssemblyName System.Windows.Forms
$p = Start-Process -FilePath '.\PeztoldPaint.exe' -PassThru
Start-Sleep -Seconds 3
$windows = [System.Windows.Forms.Form]::AllForms
Write-Host "Found $($windows.Count) windows:"
foreach($w in $windows) {
    Write-Host "  - Title: '$($w.Text)', Visible: $($w.Visible), HasHandle: $($w.Handle -ne [IntPtr]::Zero)"
}
if(-not $p.HasExited) {
    Write-Host "Process is running with PID: $($p.Id)"
    Stop-Process $p.Id -Force
} else {
    Write-Host "Process exited with code: $($p.ExitCode)"
}