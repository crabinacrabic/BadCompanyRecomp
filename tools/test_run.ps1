# Battlefield: Bad Company Recomp - Test Run & Crash Monitor (crash_hunt equivalent)
param(
    [int]$TimeoutSeconds = 95,
    [string[]]$Flags = @(),
    [switch]$KeepAlive,
    [switch]$AutomateCampaign
)

$ErrorActionPreference = "Continue"

try {
    Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class WinInput {
    [DllImport("user32.dll")]
    public static extern void keybd_event(byte bVk, byte bScan, uint dwFlags, UIntPtr dwExtraInfo);
    [DllImport("user32.dll")]
    public static extern uint MapVirtualKeyA(uint uCode, uint uMapType);
    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")]
    public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);

    public static void PressKey(byte vk) {
        byte scan = (byte)MapVirtualKeyA(vk, 0);
        keybd_event(vk, scan, 0, UIntPtr.Zero);
        System.Threading.Thread.Sleep(80);
        keybd_event(vk, scan, 2, UIntPtr.Zero);
    }
}
"@ -ErrorAction SilentlyContinue
} catch {}

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
                    } elseif ($line -match "HOOK|CACHE BYPASS|Initializing|Ready|D3D12|Mount|fix|realmemcard|XamShowDevice|sp01|level|ERROR_IO_PENDING") {
                        Write-Host " [INFO]  $line" -ForegroundColor Yellow
                    }
                }
            }
            $stream.Close()
        } catch {
            # File may be locked temporarily
        }
    }

    function Save-ScreenFrame([string]$name) {
        try {
            Add-Type -AssemblyName System.Drawing -ErrorAction SilentlyContinue
            $bmp = New-Object System.Drawing.Bitmap(1920, 1080)
            $g = [System.Drawing.Graphics]::FromImage($bmp)
            $g.CopyFromScreen(0, 0, 0, 0, $bmp.Size)
            $brainDir = "C:\Users\MSI\.gemini\antigravity\brain\37faa37a-aef7-4ec4-84f0-471f9df5e800"
            $outPath = "$brainDir\$name.png"
            $bmp.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png)
            $g.Dispose()
            $bmp.Dispose()
            Write-Host " [FRAME] Captured $name.png" -ForegroundColor DarkYellow
        } catch {
            Write-Warning "Save-ScreenFrame failed: $_"
        }
    }

    if ($AutomateCampaign -and -not $proc.HasExited) {
        if ($i -ge 46 -and $i -le 54 -and ($i % 2 -eq 0)) {
            Write-Host " [INPUT] ($i s) Pressing START (Enter / X)..." -ForegroundColor Magenta
            try {
                $hWnd = (Get-Process -Id $proc.Id).MainWindowHandle
                if ($hWnd -ne [IntPtr]::Zero) {
                    [WinInput]::ShowWindow($hWnd, 9) | Out-Null
                    [WinInput]::SetForegroundWindow($hWnd) | Out-Null
                    Start-Sleep -Milliseconds 150
                }
                [WinInput]::PressKey(0x0D) # Enter
                Start-Sleep -Milliseconds 100
                [WinInput]::PressKey(0x58) # X
            } catch {}
        } elseif ($i -ge 58 -and $i -le 64 -and ($i % 2 -eq 0)) {
            Write-Host " [INPUT] ($i s) Selecting Singleplayer (Space / A button)..." -ForegroundColor Magenta
            try {
                $hWnd = (Get-Process -Id $proc.Id).MainWindowHandle
                if ($hWnd -ne [IntPtr]::Zero) {
                    [WinInput]::ShowWindow($hWnd, 9) | Out-Null
                    [WinInput]::SetForegroundWindow($hWnd) | Out-Null
                    Start-Sleep -Milliseconds 150
                }
                [WinInput]::PressKey(0x20) # Space
                Start-Sleep -Milliseconds 100
                [WinInput]::PressKey(0xBA) # ';'
            } catch {}
        } elseif ($i -ge 68 -and $i -le 74 -and ($i % 2 -eq 0)) {
            Write-Host " [INPUT] ($i s) Confirming Difficulty Normal (Space / A button)..." -ForegroundColor Magenta
            try {
                $hWnd = (Get-Process -Id $proc.Id).MainWindowHandle
                if ($hWnd -ne [IntPtr]::Zero) {
                    [WinInput]::ShowWindow($hWnd, 9) | Out-Null
                    [WinInput]::SetForegroundWindow($hWnd) | Out-Null
                    Start-Sleep -Milliseconds 150
                }
                [WinInput]::PressKey(0x20) # Space
                Start-Sleep -Milliseconds 100
                [WinInput]::PressKey(0xBA) # ';'
            } catch {}
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
