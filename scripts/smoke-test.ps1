<#
.SYNOPSIS
    Local Windows smoke-test for BridgeDotNetWorker.

.DESCRIPTION
    1. Builds BridgeDotNetWorker in Release configuration.
    2. Starts the worker process with output redirected to temp files.
    3. Connects to the named pipe and sends PING, then CONNECT.
    4. Optionally sends a PLACE request.
    5. Prints responses and exits non-zero on any failure.
    6. Enforces an overall wall-clock timeout (OverallTimeoutSec) and prints
       diagnostics (worker stdout/stderr, bridge.log) on any failure.

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
    Defaults to 15.

.PARAMETER ConnectTimeoutMs
    Timeout per pipe connect attempt, in milliseconds. Defaults to 1000.

.PARAMETER MaxConnectRetries
    Maximum number of pipe connect attempts. Defaults to 10.

.PARAMETER ConnectRetryDelayMs
    Delay between pipe connect attempts, in milliseconds. Defaults to 2000.

.PARAMETER OverallTimeoutSec
    Maximum total wall-clock seconds the script may run. Defaults to 90.
    If exceeded the script prints diagnostics and exits non-zero.
#>

[CmdletBinding()]
param(
    [string]$PipeName            = "BridgeT4Pipe",
    [string]$ConfigPath          = "",
    [string]$PlaceRequest        = "",
    [switch]$T4SDK,
    [switch]$RequireConnect,
    [int]$ReadTimeoutSec         = 15,
    [int]$ConnectTimeoutMs       = 1000,
    [int]$MaxConnectRetries      = 10,
    [int]$ConnectRetryDelayMs    = 2000,
    [int]$OverallTimeoutSec      = 90
)

$ErrorActionPreference = "Stop"
$repoRoot   = Resolve-Path "$PSScriptRoot\.."
$proj       = Join-Path $repoRoot "dotnet\BridgeDotNetWorker\BridgeDotNetWorker.csproj"
$publishDir = Join-Path $repoRoot "dotnet\BridgeDotNetWorker\bin\Release\net8.0"

# Overall deadline ─────────────────────────────────────────────────────────────
$overallDeadline = [System.DateTime]::UtcNow.AddSeconds($OverallTimeoutSec)

function Get-RemainingSeconds {
    return [Math]::Max(0.0, ($overallDeadline - [System.DateTime]::UtcNow).TotalSeconds)
}

Write-Host "=== Smoke Test ===" -ForegroundColor Cyan
Write-Host ("Overall timeout: {0}s  (deadline {1:u} UTC)" -f $OverallTimeoutSec, $overallDeadline)

# Minimum seconds to keep as a read-timeout floor and deadline buffer ──────────
$MinReadTimeoutSec = 2
$ReadDeadlineBuffer = 1
$utf8NoBom = [System.Text.UTF8Encoding]::new($false)

# Temp files for worker output (avoid console I/O deadlock) ────────────────────
$currentPid       = [System.Diagnostics.Process]::GetCurrentProcess().Id
$workerStdoutFile = [System.IO.Path]::Combine([System.IO.Path]::GetTempPath(), "smoketest_stdout_${currentPid}.txt")
$workerStderrFile = [System.IO.Path]::Combine([System.IO.Path]::GetTempPath(), "smoketest_stderr_${currentPid}.txt")

# ── Diagnostics helper ────────────────────────────────────────────────────────
function Show-Diagnostics {
    Write-Host ""
    Write-Host "=== Diagnostics ===" -ForegroundColor Yellow

    if ($null -ne $workerProc) {
        if ($workerProc.HasExited) {
            Write-Host ("Worker process EXITED with code: {0}" -f $workerProc.ExitCode) -ForegroundColor Yellow
        } else {
            Write-Host ("Worker process still running (PID {0})" -f $workerProc.Id) -ForegroundColor Yellow
        }
    }

    if (Test-Path $workerStdoutFile) {
        Write-Host "--- Worker stdout ---" -ForegroundColor Yellow
        Get-Content $workerStdoutFile -ErrorAction SilentlyContinue | ForEach-Object { Write-Host $_ }
    } else {
        Write-Host "--- Worker stdout file not found: $workerStdoutFile ---" -ForegroundColor Yellow
    }

    if (Test-Path $workerStderrFile) {
        $errLines = Get-Content $workerStderrFile -ErrorAction SilentlyContinue
        if ($errLines) {
            Write-Host "--- Worker stderr ---" -ForegroundColor Yellow
            $errLines | ForEach-Object { Write-Host $_ }
        }
    }

    $bridgeLog = Join-Path $repoRoot "logs\bridge.log"
    if (Test-Path $bridgeLog) {
        Write-Host "--- logs/bridge.log ---" -ForegroundColor Yellow
        Get-Content $bridgeLog -ErrorAction SilentlyContinue | ForEach-Object { Write-Host $_ }
    }
}

# ── 1. Build ──────────────────────────────────────────────────────────────────
Write-Host "[1/4] Building BridgeDotNetWorker (Release) …"
$dotnet    = (Get-Command dotnet -ErrorAction Stop).Source
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

# Redirect worker output to files to prevent console I/O deadlocks.
$procSplat = @{
    PassThru                = $true
    NoNewWindow             = $true
    RedirectStandardOutput  = $workerStdoutFile
    RedirectStandardError   = $workerStderrFile
}
if (Test-Path $workerExe) {
    $procSplat['FilePath']     = $workerExe
    $procSplat['ArgumentList'] = $launchArgs
} else {
    $procSplat['FilePath']     = $dotnet
    $procSplat['ArgumentList'] = (@($workerDll) + $launchArgs)
}

$workerProc = Start-Process @procSplat

# Give the worker a moment to start
Start-Sleep -Milliseconds 500

if ($workerProc.HasExited) {
    Write-Error "Worker process exited prematurely (code $($workerProc.ExitCode))."
    Show-Diagnostics
    exit 1
}
Write-Host "      Worker started (PID $($workerProc.Id))." -ForegroundColor Green

$failed = $false
$pipe   = $null
$reader = $null
$writer = $null

# ── Send-Command ──────────────────────────────────────────────────────────────
# Must be defined before the try block so it can be called from within it.
function Send-Command([string]$cmd) {
    $remaining = Get-RemainingSeconds
    if ($remaining -le 0) {
        Write-Host ("  >> (overall timeout exceeded – cannot send '{0}')" -f $cmd) -ForegroundColor Red
        return $null
    }

    $readTimeout = [Math]::Min($ReadTimeoutSec, [Math]::Max($MinReadTimeoutSec, $remaining - $ReadDeadlineBuffer))

    Write-Host ("  >> {0}  (overall remaining: {1:F0}s, read-timeout: {2:F0}s)" -f $cmd, $remaining, $readTimeout)
    try {
        $writer.WriteLine($cmd)
    } catch {
        Write-Host ("  >> write error: {0}" -f $_) -ForegroundColor Red
        return $null
    }
    Write-Host "  >> sent OK."

    Write-Host "  << awaiting response …"
    $cts = $null
    try {
        $cts = [System.Threading.CancellationTokenSource]::new()
        $cts.CancelAfter([System.TimeSpan]::FromSeconds($readTimeout))
        $resp = $reader.ReadLineAsync($cts.Token).GetAwaiter().GetResult()
    } catch [System.OperationCanceledException] {
        Write-Host ("  << timed out after {0:F0}s – no response received." -f $readTimeout) -ForegroundColor Red
        return $null
    } catch {
        Write-Host ("  << read error: {0}" -f $_) -ForegroundColor Red
        return $null
    } finally {
        if ($cts) { $cts.Dispose() }
    }

    Write-Host "  << $resp"
    return $resp
}

try {
    # ── 3. Connect pipe & send commands ──────────────────────────────────────
    Write-Host "[3/4] Connecting to pipe '$PipeName' …"
    Add-Type -AssemblyName System.IO.Pipes

    $connected = $false

    for ($attempt = 1; $attempt -le $MaxConnectRetries; $attempt++) {
        $remaining = Get-RemainingSeconds
        if ($remaining -le 0) {
            Write-Host "  Overall timeout exceeded while waiting for pipe connection." -ForegroundColor Red
            $failed = $true
            break
        }

        if ($workerProc.HasExited) {
            Write-Host ("  Worker process exited before pipe connection (code {0})." -f $workerProc.ExitCode) -ForegroundColor Red
            $failed = $true
            break
        }

        Write-Host ("  Attempt {0}/{1}: connecting to pipe … (overall remaining: {2:F0}s)" -f $attempt, $MaxConnectRetries, $remaining)

        $pipe = [System.IO.Pipes.NamedPipeClientStream]::new(
            ".",
            $PipeName,
            [System.IO.Pipes.PipeDirection]::InOut,
            [System.IO.Pipes.PipeOptions]::None
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
        Write-Host ("Could not connect to pipe '{0}' after {1} attempt(s)." -f $PipeName, $MaxConnectRetries) -ForegroundColor Red
        $failed = $true
    } else {
        Write-Host "  Connected to pipe '$PipeName'." -ForegroundColor Green

        $reader = [System.IO.StreamReader]::new($pipe, $utf8NoBom)
        $writer = [System.IO.StreamWriter]::new($pipe, $utf8NoBom)
        $writer.AutoFlush = $true

        # PING
        $resp = Send-Command "PING"
        if ($null -eq $resp -or $resp -notmatch "^OK") {
            Write-Host "  PING failed: $resp" -ForegroundColor Red
            $failed = $true
        } else {
            Write-Host "  PING OK" -ForegroundColor Green
        }

        # CONNECT
        if ((Get-RemainingSeconds) -gt 0) {
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
        }

        # PLACE (optional)
        if ($PlaceRequest -ne "" -and (Get-RemainingSeconds) -gt 0) {
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

        # Check overall timeout before EXIT
        if ((Get-RemainingSeconds) -le 0) {
            Write-Host ("Overall timeout of {0}s exceeded!" -f $OverallTimeoutSec) -ForegroundColor Red
            $failed = $true
        }

        # EXIT – best-effort; always attempt
        Write-Host "Sending EXIT …"
        Send-Command "EXIT" | Out-Null
    }

} finally {
    if ($failed) {
        Show-Diagnostics
    }

    # Dispose pipe objects
    try { if ($writer) { $writer.Dispose() } } catch { }
    try { if ($reader) { $reader.Dispose() } } catch { }
    try { if ($pipe)   { $pipe.Dispose()   } } catch { }

    # ── 4. Tear down worker ───────────────────────────────────────────────────
    try {
        if (-not $workerProc.HasExited) {
            $workerProc.WaitForExit(3000) | Out-Null
            if (-not $workerProc.HasExited) {
                $workerProc.Kill()
            }
        }
    } catch { }

    # Clean up temp files
    try { Remove-Item $workerStdoutFile -Force -ErrorAction SilentlyContinue } catch { }
    try { Remove-Item $workerStderrFile -Force -ErrorAction SilentlyContinue } catch { }
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
