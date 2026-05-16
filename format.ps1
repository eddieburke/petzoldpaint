# PowerShell 5 — format all C sources under src/ and include/.
$ErrorActionPreference = "Stop"
$repoRoot = $PSScriptRoot
$clangFormat = Join-Path ${env:ProgramFiles} "LLVM\bin\clang-format.exe"
if (-not (Test-Path -LiteralPath $clangFormat)) {
    $clangFormat = "clang-format"
}
& $clangFormat --version
$dirs = @(
    (Join-Path $repoRoot "src"),
    (Join-Path $repoRoot "include")
)
$count = 0
foreach ($d in $dirs) {
    if (-not (Test-Path -LiteralPath $d)) { continue }
    Get-ChildItem -Path $d -Recurse -Include *.c, *.h -File | ForEach-Object {
        & $clangFormat -i $_.FullName
        $count++
    }
}
Write-Host "Formatted $count files."
