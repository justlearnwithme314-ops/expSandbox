param([switch]$Test, [switch]$Run)
$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
$cmakeExe = if ($cmakeCommand) { $cmakeCommand.Source } else { 'C:\Program Files\CMake\bin\cmake.exe' }
& $cmakeExe --preset windows
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }
& $cmakeExe --build --preset windows --parallel
if ($LASTEXITCODE -ne 0) { throw 'C++ build failed' }
if ($Test) {
    $ctestExe = Join-Path (Split-Path $cmakeExe) 'ctest.exe'
    & $ctestExe --preset windows
    if ($LASTEXITCODE -ne 0) { throw 'Core tests failed' }
}
if ($Run) { Start-Process -FilePath (Join-Path $PSScriptRoot 'build\native\bin\Release\voxel-lab.exe') -WorkingDirectory $PSScriptRoot -WindowStyle Hidden }
