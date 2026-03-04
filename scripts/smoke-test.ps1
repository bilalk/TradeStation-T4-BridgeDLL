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

.PARAMETER RequireConnect
    When present, a failed CONNECT response is treated as a fatal error (non-zero exit).
    By default CONNECT failure is only a warning (real connector may not have creds).

.PARAMETER ReadTimeoutSec
    Seconds to wait for a response from the pipe before treating it as a timeout error.
    Defaults to 10.

.PARAMETER ConnectTimeoutMs
    Timeout per pipe connect attempt, in milliseconds. Defaults to 1000.

.PARAMETER MaxConnectRetries
    Maximum number of pipe connect attempts. Defaults to 10.

.PARAMETER ConnectRetryDelayMs
    Delay between pipe connect attempts, in milliseconds. Defaults to 2000.

.PARAMETER WriteTimeoutSec
    Seconds to wait for a single pipe write to complete before treating it as a timeout error.
    Defaults to 10.

.PARAMETER OverallTimeoutSec
    Hard overall timeout in seconds for the pipe-connect-and-communicate phase (excludes build).
    Defaults to 120.
#>

[CmdletBinding()]
param(
    [string]$PipeName            = "BridgeT4Pipe",
    [string]$ConfigPath          = "",
    [string]$PlaceRequest        = "",
    [switch]$T4SDK,
    [switch]$RequireConnect,
    [int]$ReadTimeoutSec         = 10,
    [int]$ConnectTimeoutMs       = 1000,
    [int]$MaxConnectRetries      = 10,
    [int]$ConnectRetryDelayMs    = 2000,
    [int]$WriteTimeoutSec        = 10,
    [int]$OverallTimeoutSec      = 120
)

$ErrorActionPreference = "Stop"
$sw           = [System.Diagnostics.Stopwatch]::StartNew()
$workerStdout = $null   # set below; initialised here so finally-block cleanup is always safe
$workerStderr = $null
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

# Redirect worker output to temp files so we can dump them on failure for diagnostics.
$workerStdout = [System.IO.Path]::Combine([System.IO.Path]::GetTempPath(), "smoke-worker-out-$PID.txt")
$workerStderr = [System.IO.Path]::Combine([System.IO.Path]::GetTempPath(), "smoke-worker-err-$PID.txt")

if (Test-Path $workerExe) {
    $workerProc = Start-Process -FilePath $workerExe `
        -ArgumentList $launchArgs `
        -PassThru -NoNewWindow `
        -RedirectStandardOutput $workerStdout `
        -RedirectStandardError  $workerStderr
} else {
    $workerProc = Start-Process -FilePath $dotnet `
        -ArgumentList (@($workerDll) + $launchArgs) `
        -PassThru -NoNewWindow `
        -RedirectStandardOutput $workerStdout `
        -RedirectStandardError  $workerStderr
}

# Give the worker a moment to start
Start-Sleep -Milliseconds 500

if ($workerProc.HasExited) {
    Write-Error "Worker process exited prematurely (code $($workerProc.ExitCode))."
    exit 1
}
Write-Host "      Worker started (PID $($workerProc.Id))." -ForegroundColor Green

$failed     = $false
$pipeBroken = $false
$pipe       = $null
$reader     = $null

try {
    # ── 3. Connect pipe & send commands ──────────────────────────────────────
    Write-Host "[3/4] Connecting to pipe '$PipeName' …"
    Add-Type -AssemblyName System.IO.Pipes

    $connected = $false

    for ($attempt = 1; $attempt -le $MaxConnectRetries; $attempt++) {
        if ($workerProc.HasExited) {
            Write-Error "Worker process exited before pipe connection (code $($workerProc.ExitCode))."
            $failed = $true
            break
        }

        # NOTE: use ${MaxConnectRetries} to avoid PowerShell parsing issues near ':' in strings.
        Write-Host ("  Attempt {0}/{1}: connecting to pipe …" -f $attempt, $MaxConnectRetries)

        $pipe = [System.IO.Pipes.NamedPipeClientStream]::new(
            ".",                # server name (local)
            $PipeName,
            [System.IO.Pipes.PipeDirection]::InOut,
            [System.IO.Pipes.PipeOptions]::Asynchronous
        )

        try {
            $pipe.Connect($ConnectTimeoutMs)
            $connected = $true
            break
        } catch {
            try { $pipe.Dispose() } catch { }
            $pipe = $null

            if ($attempt -lt $MaxConnectRetries) {
                Start-Sleep -Milliseconds $ConnectRetryDelayMs
            }
        }
    }

    if (-not $connected) {
        Write-Error "Could not connect to pipe '$PipeName' after $MaxConnectRetries attempt(s)."
        $failed = $true
        return
    }

    $reader = [System.IO.StreamReader]::new($pipe, [System.Text.Encoding]::UTF8)

    function Send-Command([string]$cmd) {
        # Bail out fast if the pipe is already known-broken or overall timeout exceeded.
        if ($script:pipeBroken) {
            Write-Host ("  (skipping '{0}': pipe broken)" -f $cmd) -ForegroundColor Yellow
            return $null
        }
        if ($sw.Elapsed.TotalSeconds -gt $OverallTimeoutSec) {
            Write-Host ("  (skipping '{0}': overall timeout {1}s exceeded; elapsed {2:F0}s)" -f $cmd, $OverallTimeoutSec, $sw.Elapsed.TotalSeconds) -ForegroundColor Yellow
            $script:failed = $true
            return $null
        }

        Write-Host "  >> $cmd"

        # Write command to pipe using async I/O with a hard timeout.
        # Avoids the blocking StreamWriter.WriteLine() which can deadlock on CI.
        $bytes     = [System.Text.Encoding]::UTF8.GetBytes($cmd + "`r`n")
        $writeTask = $pipe.WriteAsync($bytes, 0, $bytes.Length)
        if (-not $writeTask.Wait($WriteTimeoutSec * 1000)) {
            Write-Host ("  >> write timed out after {0}s; worker HasExited={1}" -f $WriteTimeoutSec, $workerProc.HasExited) -ForegroundColor Red
            $script:pipeBroken = $true
            return $null
        }
        if ($writeTask.IsFaulted) {
            Write-Host ("  >> write error: {0}" -f $writeTask.Exception.GetBaseException().Message) -ForegroundColor Red
            $script:pipeBroken = $true
            return $null
        }
        Write-Host "  >> (sent, awaiting response)"

        # Read response with timeout.  We track whether a read task is outstanding so we
        # never call ReadLineAsync() a second time while a previous call is still pending
        # (doing so causes an undefined-behaviour deadlock in StreamReader).
        $readTask = $reader.ReadLineAsync()
        try {
            if (-not $readTask.Wait([System.TimeSpan]::FromSeconds($ReadTimeoutSec))) {
                Write-Host ("  << no response after {0}s; worker HasExited={1}" -f $ReadTimeoutSec, $workerProc.HasExited) -ForegroundColor Red
                $script:pipeBroken = $true
                return $null
            }
        } catch {
            Write-Host ("  << read error: {0}" -f $_) -ForegroundColor Red
            $script:pipeBroken = $true
            return $null
        }

        $resp = $readTask.Result
        Write-Host "  << $resp"
        return $resp
    }

    # PING
    $resp = Send-Command "PING"
    if ($null -eq $resp -or $resp -notmatch "^OK") {
        Write-Host "  PING failed: $resp" -ForegroundColor Red
        $failed = $true
    } else {
        Write-Host "  PING OK" -ForegroundColor Green
    }

    # CONNECT
    $resp = Send-Command "CONNECT"
    if ($null -eq $resp -or $resp -notmatch "^OK") {
        if ($RequireConnect) {
            Write-Host "  CONNECT failed (fatal): $resp" -ForegroundColor Red
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
        if ($null -eq $resp -or $resp -notmatch "^OK") {
            Write-Host "  PLACE failed: $resp" -ForegroundColor Yellow
        } else {
            Write-Host "  PLACE OK" -ForegroundColor Green
        }
    } else {
        Write-Host "[4/4] Skipping PLACE (no -PlaceRequest supplied)."
    }

    # EXIT
    Send-Command "EXIT" | Out-Null

} finally {
    # dispose pipe objects
    try { if ($reader) { $reader.Dispose() } } catch { }
    try { if ($pipe)   { $pipe.Dispose()   } } catch { }

    # ── 4. Tear down worker ───────────────────────────────────────────────────
    try {
        if ($workerProc -and -not $workerProc.HasExited) {
            $workerProc.WaitForExit(3000) | Out-Null
            if (-not $workerProc.HasExited) {
                $workerProc.Kill()
            }
        }
    } catch { }

    # ── 5. Diagnostics on failure / timeout ───────────────────────────────────
    if ($failed -or $pipeBroken) {
        Write-Host ""
        Write-Host "=== FAILURE DIAGNOSTICS ===" -ForegroundColor Cyan
        $elapsed = [int]$sw.Elapsed.TotalSeconds
        Write-Host ("  Elapsed  : {0}s (limit {1}s)" -f $elapsed, $OverallTimeoutSec)
        if ($workerProc) {
            $exitInfo = if ($workerProc.HasExited) { "exited (code $($workerProc.ExitCode))" } else { "still running" }
            Write-Host ("  Worker   : PID {0} — {1}" -f $workerProc.Id, $exitInfo)
        }
        foreach ($entry in @(
            @{ File = $workerStdout; Label = "worker stdout" },
            @{ File = $workerStderr; Label = "worker stderr" }
        )) {
            if ($entry.File -and (Test-Path $entry.File -ErrorAction SilentlyContinue)) {
                $lines = Get-Content $entry.File -ErrorAction SilentlyContinue | Select-Object -Last 30
                if ($lines) {
                    Write-Host ""
                    Write-Host ("--- {0} (last 30 lines) ---" -f $entry.Label)
                    $lines | ForEach-Object { Write-Host "  $_" }
                }
            }
        }
        $logsDir = Join-Path $repoRoot "logs"
        if (Test-Path $logsDir) {
            Write-Host ""
            Write-Host "--- logs/ ---"
            Get-ChildItem $logsDir -Recurse -File -ErrorAction SilentlyContinue | ForEach-Object {
                Write-Host "  $($_.Name):"
                Get-Content $_.FullName -ErrorAction SilentlyContinue |
                    Select-Object -Last 10 | ForEach-Object { Write-Host "    $_" }
            }
        }
    }

    # Clean up temp log files
    foreach ($f in @($workerStdout, $workerStderr)) {
        try { if ($f) { Remove-Item $f -Force -ErrorAction SilentlyContinue } } catch { }
    }
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
