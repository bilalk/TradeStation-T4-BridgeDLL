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

.PARAMETER RequireConnect
    When present, a failed CONNECT response is treated as a fatal error (non-zero exit).
    By default CONNECT failure is only a warning (real connector may not have creds).

.PARAMETER ReadTimeoutSec
    Seconds to wait for a response from the pipe before treating it as a timeout error.
    Defaults to 10.

.PARAMETER MaxRetries
    Maximum number of pipe-connection attempts before giving up.
    Between attempts the script checks whether the worker has already exited (fail-fast)
    and waits RetryDelayMs milliseconds.  Defaults to 10.

.PARAMETER RetryDelayMs
    Milliseconds to wait between consecutive pipe-connection attempts.
    Defaults to 2000 (2 s).

.EXAMPLE
    .\scripts\smoke-test.ps1

.EXAMPLE
    .\scripts\smoke-test.ps1 -PipeName MyPipe -ConfigPath .\config\bridge.json

.EXAMPLE
    .\scripts\smoke-test.ps1 -PlaceRequest "PLACE ESZ4 BUY 1 4500.00 LIMIT"

.EXAMPLE
    .\scripts\smoke-test.ps1 -T4SDK

.EXAMPLE
    .\scripts\smoke-test.ps1 -RequireConnect
#>

[CmdletBinding()]
param(
    [string]$PipeName       = "BridgeT4Pipe",
    [string]$ConfigPath     = "",
    [string]$PlaceRequest   = "",
    [switch]$T4SDK,
    [switch]$RequireConnect,
    [int]$ReadTimeoutSec    = 10,
    [int]$MaxRetries        = 10,
    [int]$RetryDelayMs      = 2000
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

# Temp files to capture worker output for diagnostics (automatically cleaned up in finally)
$workerStdout = [System.IO.Path]::GetTempFileName()
$workerStderr = [System.IO.Path]::GetTempFileName()

# The pipe name is passed explicitly to both the worker (--pipe) and the client below,
# ensuring they always use the same value from the single $PipeName variable.
$launchArgs = @("--pipe", $PipeName)
if ($ConfigPath -ne "") {
    $launchArgs += "--config"
    $launchArgs += $ConfigPath
}

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

Write-Host "      Worker started (PID $($workerProc.Id))." -ForegroundColor Green

$failed = $false

# Helper: emit worker stdout/stderr and recent log files when diagnosing failures.
function Show-WorkerDiagnostics {
    Write-Host "--- Worker stdout (last 50 lines) ---" -ForegroundColor Yellow
    if (Test-Path $workerStdout) {
        (Get-Content $workerStdout -ErrorAction SilentlyContinue) |
            Select-Object -Last 50 | ForEach-Object { Write-Host $_ }
    } else {
        Write-Host "  (no stdout captured)"
    }
    Write-Host "--- Worker stderr (last 50 lines) ---" -ForegroundColor Yellow
    if (Test-Path $workerStderr) {
        (Get-Content $workerStderr -ErrorAction SilentlyContinue) |
            Select-Object -Last 50 | ForEach-Object { Write-Host $_ }
    } else {
        Write-Host "  (no stderr captured)"
    }
    Write-Host "--- Log files ---" -ForegroundColor Yellow
    $logDir = Join-Path $repoRoot "logs"
    if (Test-Path $logDir) {
        Get-ChildItem $logDir -Recurse -File -ErrorAction SilentlyContinue | ForEach-Object {
            Write-Host "  $($_.FullName)"
            Get-Content $_.FullName -ErrorAction SilentlyContinue |
                Select-Object -Last 20 | ForEach-Object { Write-Host "    $_" }
        }
    } else {
        Write-Host "  (no logs directory)"
    }
}

try {
    # ── 3. Connect pipe & send commands ──────────────────────────────────────
    Write-Host "[3/4] Connecting to pipe '$PipeName' …"

    Add-Type -AssemblyName System.IO.Pipes

    # Retry/backoff loop: attempt to connect to the named pipe up to $MaxRetries times.
    # Before each attempt we check whether the worker has already exited so we can
    # fail fast with useful diagnostics rather than waiting out all retries.
    $pipe = $null
    $connected = $false
    for ($attempt = 1; $attempt -le $MaxRetries; $attempt++) {
        # Fail fast: if the worker has already exited there is no point retrying.
        if ($workerProc.HasExited) {
            Write-Host "  Worker exited prematurely (code $($workerProc.ExitCode))." -ForegroundColor Red
            Show-WorkerDiagnostics
            $failed = $true
            break
        }

        Write-Host "  Attempt $attempt/$MaxRetries: connecting to pipe '$PipeName' …"
        # A fresh NamedPipeClientStream must be created for each attempt because
        # Connect() renders the stream object unusable after a timeout.
        $pipe = [System.IO.Pipes.NamedPipeClientStream]::new(
            ".",
            $PipeName,
            [System.IO.Pipes.PipeDirection]::InOut,
            [System.IO.Pipes.PipeOptions]::None
        )
        try {
            $pipe.Connect(1000)   # 1000 ms (1-second) timeout per attempt
            $connected = $true
            Write-Host "      Connected on attempt $attempt." -ForegroundColor Green
            break
        } catch {
            $pipe.Dispose()
            $pipe = $null
            if ($attempt -lt $MaxRetries) {
                $retryDelaySec = [math]::Round($RetryDelayMs / 1000.0, 1)
                Write-Host "  Not ready yet; retrying in ${retryDelaySec}s …"
                Start-Sleep -Milliseconds $RetryDelayMs
            }
        }
    }

    if (-not $connected -and -not $failed) {
        Write-Host "  Could not connect to pipe '$PipeName' after $MaxRetries attempts." -ForegroundColor Red
        Show-WorkerDiagnostics
        $failed = $true
    }

    if ($failed) { return }

    $reader = [System.IO.StreamReader]::new($pipe, [System.Text.Encoding]::UTF8)
    $writer = [System.IO.StreamWriter]::new($pipe, [System.Text.Encoding]::UTF8)
    $writer.AutoFlush = $true

    function Send-Command([string]$cmd) {
        Write-Host "  >> $cmd"
        $writer.WriteLine($cmd)
        $task = $reader.ReadLineAsync()
        try {
            if (-not $task.Wait([System.TimeSpan]::FromSeconds($ReadTimeoutSec))) {
                Write-Host "  << (no response after ${ReadTimeoutSec}s)" -ForegroundColor Red
                return $null
            }
        } catch {
            Write-Host "  << (read error: $_)" -ForegroundColor Red
            return $null
        }
        $resp = $task.Result
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

    $pipe.Dispose()
    $reader.Dispose()
    $writer.Dispose()

} finally {
    # ── 4. Tear down worker ───────────────────────────────────────────────────
    # Dispose the pipe (may already be disposed if connection was never established)
    if ($null -ne $pipe) {
        try { $pipe.Dispose() } catch { }
    }

    try {
        if (-not $workerProc.HasExited) {
            $workerProc.WaitForExit(3000) | Out-Null
            if (-not $workerProc.HasExited) {
                $workerProc.Kill()
            }
        }
    } catch { }

    # Clean up temporary output-capture files
    Remove-Item -Path $workerStdout -ErrorAction SilentlyContinue
    Remove-Item -Path $workerStderr -ErrorAction SilentlyContinue
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
