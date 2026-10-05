# Battlefield: Bad Company Recomp - Launch Script (F5 equivalent)
param(
    [string[]]$Flags = @(),
    [string]$Config = "local-win-relwithdebinfo"
)

$dir = "C:\Recompiles\Bad Company\out\build\$Config"
$exePath = "$dir\badcompanyrecomp.exe"

if (-not (Test-Path $exePath)) {
    Write-Error "Executable not found: $exePath. Run tools/build.ps1 first!"
    exit 1
}

Write-Host "Stopping any running badcompanyrecomp processes..." -ForegroundColor Yellow
Get-Process badcompanyrecomp -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 1200

Write-Host "Launching $exePath..." -ForegroundColor Green
$startParams = @{
    FilePath = $exePath
    WorkingDirectory = $dir
    PassThru = $true
}
if ($Flags -and $Flags.Count -gt 0) {
    $startParams["ArgumentList"] = $Flags
}

$p = Start-Process @startParams
Write-Host ("Process started: PID {0}, Arguments: '{1}'" -f $p.Id, ($Flags -join ' ')) -ForegroundColor Cyan
