# Battlefield: Bad Company Recomp - Test Run & Crash Monitor (crash_hunt equivalent)
param(
    [int]$TimeoutSeconds = 25,
    [string[]]$Flags = @(),
    [switch]$KeepAlive
)

$ErrorActionPreference = "Continue"
$projectDir = "C:\Recompiles\Bad Company"
$dir = "$projectDir\out\build\local-win-relwithdebinfo"
$exePath = "$dir\badcompanyrecomp.exe"
$logsDir = "$dir\logs"
if (-not (Test-Path $logsDir)) {
    $logsDir = "$projectDir\logs"
}

if (-not (Test-Path $exePath)) {
    Write-Error "Executable not found: $exePath! Run tools\build.ps1 first."
    exit 1
}

Write-Host "=== [1/4] Preparing Test Environment ===" -ForegroundColor Cyan
Get-Process badcompanyrecomp -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 1200

$startTime = Get-Date

Write-Host "=== [2/4] Starting $exePath ===" -ForegroundColor Cyan
$startParams = @{
    FilePath = $exePath
    WorkingDirectory = $dir
    PassThru = $true
}
if ($Flags -and $Flags.Count -gt 0) {
    $startParams["ArgumentList"] = $Flags
}

$proc = Start-Process @startParams
if (-not $proc) {
    Write-Error "Failed to launch process $exePath"
    exit 1
}
Write-Host ("Process started with PID: {0}" -f $proc.Id) -ForegroundColor Green

Start-Sleep -Seconds 2
$logFile = $null
if (Test-Path $logsDir) {
    $logFile = Get-ChildItem -Path $logsDir -Filter "badcompanyrecomp_*.log" |
               Where-Object { $_.LastWriteTime -ge $startTime.AddSeconds(-2) } |
               Sort-Object LastWriteTime -Descending |
               Select-Object -First 1
}

if ($logFile) {
    Write-Host ("Detected log file: {0}" -f $logFile.FullName) -ForegroundColor DarkCyan
} else {
    Write-Warning "Log file not yet located in $logsDir, continuing process monitor..."
}

Write-Host ("=== [3/4] Monitoring Execution (Timeout: {0}s) ===" -f $TimeoutSeconds) -ForegroundColor Cyan
$crashed = $false
$lastReadOffset = 0
$checkInterval = 1

for ($i = 0; $i -lt $TimeoutSeconds; $i += $checkInterval) {
    Start-Sleep -Seconds $checkInterval
    
    if (-not $logFile -and (Test-Path $logsDir)) {
        $logFile = Get-ChildItem -Path $logsDir -Filter "badcompanyrecomp_*.log" |
                   Sort-Object LastWriteTime -Descending |
                   Select-Object -First 1
        if ($logFile) {
            Write-Host ("Detected log file: {0}" -f $logFile.FullName) -ForegroundColor DarkCyan
        }
    }

    if ($logFile -and (Test-Path $logFile.FullName)) {
        try {
            $stream = [System.IO.File]::Open($logFile.FullName, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
            if ($stream.Length -gt $lastReadOffset) {
                $stream.Seek($lastReadOffset, [System.IO.SeekOrigin]::Begin) | Out-Null
                $reader = New-Object System.IO.StreamReader($stream)
                $newContent = $reader.ReadToEnd()
                $lastReadOffset = $stream.Length
                $reader.Close()
                
                $lines = $newContent -split "`r?`n"
                foreach ($line in $lines) {
                    if ($line -match "CRASH|Unhandled guest|FATAL|FROSTBITE|ExceptionCode|\*\*\* STOP:") {
                        Write-Host " [ALERT] $line" -ForegroundColor Red
                        $crashed = $true
                    } elseif ($line -match "HOOK|CACHE BYPASS|Initializing|Ready|D3D12|Mount") {
                        Write-Host " [INFO]  $line" -ForegroundColor Yellow
                    }
                }
            }
            $stream.Close()
        } catch {
            # File may be locked temporarily
        }
    }

    if ($proc.HasExited) {
        $crashed = $true
        Write-Host ("Process PID {0} exited with code {1} after {2}s." -f $proc.Id, $proc.ExitCode, $i) -ForegroundColor Red
        break
    }
}

$elapsed = [int]((Get-Date) - $startTime).TotalSeconds
Write-Host "=== [4/4] Execution Summary ===" -ForegroundColor Cyan

$crashArtifacts = @(
    "$logsDir\crash_stack.txt",
    "$logsDir\bugcheck.txt",
    "$logsDir\crash.dmp"
)

foreach ($art in $crashArtifacts) {
    if (Test-Path $art) {
        $artItem = Get-Item $art
        if ($artItem.LastWriteTime -ge $startTime.AddSeconds(-2)) {
            Write-Host ("Crash artifact found: {0} ({1:N0} bytes)" -f $artItem.Name, $artItem.Length) -ForegroundColor Red
            if ($artItem.Extension -eq ".txt") {
                Get-Content $art -Tail 30 | ForEach-Object { Write-Host "   $_" -ForegroundColor DarkRed }
            }
        }
    }
}

if ($crashed) {
    Write-Host ("Result: Process CRASHED or exited prematurely after {0}s." -f $elapsed) -ForegroundColor Red
    if ($logFile) {
        Write-Host ("Tail of log ({0}):" -f $logFile.Name) -ForegroundColor Yellow
        Get-Content $logFile.FullName -Tail 25 -ErrorAction SilentlyContinue | ForEach-Object { Write-Host "   $_" }
    }
} else {
    Write-Host ("Result: Process running STABLE after {0}s (PID: {1})." -f $elapsed, $proc.Id) -ForegroundColor Green
    if (-not $KeepAlive) {
        Write-Host "Stopping test process..." -ForegroundColor Yellow
        $proc | Stop-Process -Force
    }
}
