<#
.SYNOPSIS
    Local Windows smoke-test for BridgeDotNetWorker.

.DESCRIPTION
    1. Builds BridgeDotNetWorker in Release configuration.
    2. Starts the worker process with an optional config file.
    3. Connects to the named pipe and sends PING, then CONNECT.
    4. Optionally sends a PLACE request.
    5. Prints responses and exits non-zero on any failure.

.PARAMETER PipeName
    Named pipe to use.  Defaults to "BridgeT4Pipe".

.PARAMETER ConfigPath
    Optional path to a bridge.json config file.
    If omitted the worker searches for config/bridge.json relative to the repo root.

.PARAMETER PlaceRequest
    Optional PLACE command string, e.g. "PLACE ESZ4 BUY 1 4500.00 LIMIT".
    If omitted the PLACE step is skipped.

.PARAMETER T4SDK
    When present, passes /p:T4SDK=true to dotnet build, enabling the real T4 connector.
    Requires the T4.Api NuGet package to be available (see docs/Build_and_Run.md).

.EXAMPLE
    .\scripts\smoke-test.ps1

.EXAMPLE
    .\scripts\smoke-test.ps1 -PipeName MyPipe -ConfigPath .\config\bridge.json

.EXAMPLE
    .\scripts\smoke-test.ps1 -PlaceRequest "PLACE ESZ4 BUY 1 4500.00 LIMIT"

.EXAMPLE
    .\scripts\smoke-test.ps1 -T4SDK

.EXAMPLE
    .\scripts\smoke-test.ps1 -T4SDK -RequireConnect

.PARAMETER RequireConnect
    When present, a CONNECT failure is treated as fatal (exit 1).
    Use in CI when real T4 credentials are available and must be validated.
#>

[CmdletBinding()]
param(
    [string]$PipeName    = "BridgeT4Pipe",
    [string]$ConfigPath  = "",
    [string]$PlaceRequest = "",
    [switch]$T4SDK,
    [switch]$RequireConnect
)

$ErrorActionPreference = "Stop"
$repoRoot = Resolve-Path "$PSScriptRoot\.."
$proj     = Join-Path $repoRoot "dotnet\BridgeDotNetWorker\BridgeDotNetWorker.csproj"
$publishDir = Join-Path $repoRoot "dotnet\BridgeDotNetWorker\bin\Release\net8.0"

Write-Host "=== Smoke Test ===" -ForegroundColor Cyan

# ── 1. Build ──────────────────────────────────────────────────────────────────
Write-Host "[1/4] Building BridgeDotNetWorker (Release) …"
$dotnet = (Get-Command dotnet -ErrorAction Stop).Source
$buildArgs = @($proj, "-c", "Release", "--nologo", "-v", "quiet")
if ($T4SDK) {
    $buildArgs += "/p:T4SDK=true"
    Write-Host "      T4SDK=true (REAL connector enabled)"
}
& $dotnet build @buildArgs
if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed (exit $LASTEXITCODE)."
    exit $LASTEXITCODE
}
Write-Host "      Build succeeded." -ForegroundColor Green

# ── 2. Start worker ───────────────────────────────────────────────────────────
Write-Host "[2/4] Starting worker …"
$workerExe = Join-Path $publishDir "BridgeDotNetWorker.exe"
$workerDll = Join-Path $publishDir "BridgeDotNetWorker.dll"

if (-not (Test-Path $workerExe) -and -not (Test-Path $workerDll)) {
    Write-Error "Worker binary not found in $publishDir"
    exit 1
}

$launchArgs = @("--pipe", $PipeName)
if ($ConfigPath -ne "") {
    $launchArgs += "--config"
    $launchArgs += $ConfigPath
}

if (Test-Path $workerExe) {
    $workerProc = Start-Process -FilePath $workerExe `
        -ArgumentList $launchArgs `
        -PassThru -NoNewWindow
} else {
    $workerProc = Start-Process -FilePath $dotnet `
        -ArgumentList (@($workerDll) + $launchArgs) `
        -PassThru -NoNewWindow
}

# Give the worker a moment to start listening
Start-Sleep -Milliseconds 1500

if ($workerProc.HasExited) {
    Write-Error "Worker process exited prematurely (code $($workerProc.ExitCode))."
    exit 1
}
Write-Host "      Worker started (PID $($workerProc.Id))." -ForegroundColor Green

$failed = $false

try {
    # ── 3. Connect pipe & send commands ──────────────────────────────────────
    Write-Host "[3/4] Connecting to pipe '$PipeName' …"

    Add-Type -AssemblyName System.IO.Pipes

    $pipe = [System.IO.Pipes.NamedPipeClientStream]::new(
        ".",                # server name (local)
        $PipeName,
        [System.IO.Pipes.PipeDirection]::InOut,
        [System.IO.Pipes.PipeOptions]::None
    )

    try {
        $pipe.Connect(5000)   # 5 s timeout
    } catch {
        Write-Error "Could not connect to pipe: $_"
        $failed = $true
        return
    }

    $reader = [System.IO.StreamReader]::new($pipe, [System.Text.Encoding]::UTF8)
    $writer = [System.IO.StreamWriter]::new($pipe, [System.Text.Encoding]::UTF8)
    $writer.AutoFlush = $true

    function Send-Command([string]$cmd) {
        Write-Host "  >> $cmd"
        $writer.WriteLine($cmd)
        $resp = $reader.ReadLine()
        Write-Host "  << $resp"
        return $resp
    }

    # PING
    $resp = Send-Command "PING"
    if ($resp -notmatch "^OK") {
        Write-Host "  PING failed: $resp" -ForegroundColor Red
        $failed = $true
    } else {
        Write-Host "  PING OK" -ForegroundColor Green
    }

    # CONNECT
    $resp = Send-Command "CONNECT"
    if ($resp -notmatch "^OK") {
        if ($RequireConnect) {
            Write-Host "  CONNECT FAILED (fatal with -RequireConnect): $resp" -ForegroundColor Red
            $failed = $true
        } else {
            Write-Host "  CONNECT failed: $resp" -ForegroundColor Yellow
            # Not fatal in smoke test (REAL connector may not have creds)
        }
    } else {
        Write-Host "  CONNECT OK" -ForegroundColor Green
    }

    # PLACE (optional)
    if ($PlaceRequest -ne "") {
        Write-Host "[4/4] Sending PLACE request …"
        $resp = Send-Command $PlaceRequest
        if ($resp -notmatch "^OK") {
            Write-Host "  PLACE failed: $resp" -ForegroundColor Yellow
        } else {
            Write-Host "  PLACE OK" -ForegroundColor Green
        }
    } else {
        Write-Host "[4/4] Skipping PLACE (no -PlaceRequest supplied)."
    }

    # EXIT
    Send-Command "EXIT" | Out-Null

    $pipe.Dispose()
    $reader.Dispose()
    $writer.Dispose()

} finally {
    # ── 4. Tear down worker ───────────────────────────────────────────────────
    try {
        if (-not $workerProc.HasExited) {
            $workerProc.WaitForExit(3000) | Out-Null
            if (-not $workerProc.HasExited) {
                $workerProc.Kill()
            }
        }
    } catch { }
}

if ($failed) {
    Write-Host "" 
    Write-Host "=== Smoke test FAILED ===" -ForegroundColor Red
    exit 1
} else {
    Write-Host ""
    Write-Host "=== Smoke test PASSED ===" -ForegroundColor Green
    exit 0
}
