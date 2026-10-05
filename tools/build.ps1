# Battlefield: Bad Company Recomp - Build Script (F7 equivalent)
# Incremental build via Visual Studio 18 DevShell and Clang/LLVM
param(
    [string]$Config = "local-win-relwithdebinfo"
)

$t0 = Get-Date
$projectDir = "C:\Recompiles\Bad Company"
$vs = "C:\Program Files\Microsoft Visual Studio\18\Community"
$devShellDll = "$vs\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"

Write-Host "=== [1/3] Initializing Visual Studio DevShell ===" -ForegroundColor Cyan
if (Test-Path $devShellDll) {
    $env:PATH = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer;$env:PATH"
    Import-Module $devShellDll
    Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments "-arch=x64 -host_arch=x64" 2>&1 | Out-Null
    $env:PATH = "$vs\VC\Tools\Llvm\x64\bin;$env:PATH"
} else {
    Write-Warning "DevShell.dll not found at $devShellDll. Falling back to current PATH."
}

Set-Location $projectDir
$buildDir = "out/build/$Config"
Write-Host "=== [2/3] Building with CMake ($buildDir) ===" -ForegroundColor Cyan

$buildOutput = cmake --build $buildDir 2>&1
$exitCode = $LASTEXITCODE

# Filter essential lines
$filtered = $buildOutput | Select-String -Pattern "error|warning: |FAILED|Linking|ninja: no work"
if ($filtered) {
    $filtered | Select-Object -Last 40 | ForEach-Object { Write-Host $_.Line }
} else {
    $buildOutput | Select-Object -Last 10 | ForEach-Object { Write-Host $_ }
}

$elapsed = [int]((Get-Date) - $t0).TotalSeconds
Write-Host "=== [3/3] Build Summary ===" -ForegroundColor Cyan
if ($exitCode -eq 0) {
    Write-Host "Build SUCCESS in $elapsed s (exit $exitCode)" -ForegroundColor Green
    $exe = Get-Item "$buildDir\badcompanyrecomp.exe" -ErrorAction SilentlyContinue
    if ($exe) {
        Write-Host ("Executable: {0} ({1:N0} bytes, {2})" -f $exe.FullName, $exe.Length, $exe.LastWriteTime) -ForegroundColor Yellow
    }
} else {
    Write-Host "Build FAILED in $elapsed s (exit $exitCode)" -ForegroundColor Red
    exit $exitCode
}
