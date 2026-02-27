# Build and Run Guide

## Prerequisites

- **Windows 10/11 x64**
- **Visual Studio 2022** (Community, Professional, or Enterprise) with the "Desktop development with C++" workload installed
- **Windows SDK 10.0** (installed with the workload above)
- **.NET 8 SDK** (for the `BridgeDotNetWorker` component; download from <https://dotnet.microsoft.com/download>)

---

## Building in Visual Studio 2022

1. Open Visual Studio 2022.
2. Choose **File → Open → Project/Solution**.
3. Navigate to the repository root and open `TradeStation-T4-BridgeDLL.sln`.
4. In the **Solution Configurations** toolbar, select **Release** and **x64**.
5. Press **Ctrl+Shift+B** (or **Build → Build Solution**).

### Output Files

After a successful Release build:

| File | Location |
|------|----------|
| `BridgeCore.lib` | `x64\Release\BridgeCore.lib` |
| `BridgeDLL.dll` | `x64\Release\BridgeDLL.dll` |
| `BridgeDLL.lib` | `x64\Release\BridgeDLL.lib` |
| `BridgeTestConsole.exe` | `x64\Release\BridgeTestConsole.exe` |
| `BridgeCoreTests.exe` | `x64\Release\BridgeCoreTests.exe` |

---

## Building from the Command Line

Use the provided PowerShell script:

```powershell
.\scripts\build-windows.ps1 -Configuration Release -Platform x64
```

Or directly with MSBuild (from a Visual Studio 2022 Developer PowerShell):

```powershell
msbuild TradeStation-T4-BridgeDLL.sln /m /p:Configuration=Release /p:Platform=x64
```

---

## Running Unit Tests

After building, run the unit test executable:

```powershell
.\x64\Release\BridgeCoreTests.exe
```

A passing run prints `[PASS]` for each check and exits with code `0`. Any failure prints `[FAIL]` and exits with a non-zero code.

---

## Running the Smoke Test Console

```powershell
# Copy BridgeDLL.dll next to the test console, or ensure it is on PATH
.\x64\Release\BridgeTestConsole.exe
```

The console dynamically loads `BridgeDLL.dll` and exercises each exported function.

---

## Configuration

Copy `config/bridge.example.json` to `config/bridge.json` and edit as needed
(`config/bridge.json` is gitignored so your credentials are never committed):

```json
{
  "adapterType": "MOCK",
  "logFilePath": "logs/bridge.log",
  "logToConsole": false,
  "connector": "STUB",
  "t4Host": "uhfix-sim.t4login.com",
  "t4Port": 10443,
  "t4Username": "YOUR_SIMULATOR_USERNAME",
  "pipeName": "BridgeT4Pipe"
}
```

- **adapterType**: `MOCK` (default), `FIX` (stub, not yet implemented), `DOTNET` (stub, not yet implemented).
- **logFilePath**: Path to the log file. The directory is created automatically.
- **logToConsole**: Set to `true` to also print log lines to stdout.
- **connector**: `STUB` (default) or `REAL` (see below). Can also be set via `BRIDGE_CONNECTOR` env var.
- **t4Host / t4Port**: T4 simulator endpoint. Defaults: `uhfix-sim.t4login.com:10443`.
- **t4Username**: Your T4 simulator username. Can also be set via `T4_USERNAME` env var.
- **pipeName**: Named pipe the worker listens on. Can also be set via `BRIDGE_PIPE_NAME` env var.

> **Secrets** – never store `t4Password` or `t4LicenseKey` in the JSON file.
> Set these via environment variables instead:
> ```powershell
> $env:T4_PASSWORD    = "your-password"
> $env:T4_LICENSE_KEY = "your-license-key"
> ```

If `config/bridge.json` is not found, the engine uses built-in defaults (MOCK adapter, `logs/bridge.log`).

---

## BridgeDotNetWorker

`dotnet/BridgeDotNetWorker/` is a .NET 8 console application that listens on a named pipe
and forwards commands to a T4 connector.

### Building the worker

```powershell
dotnet build dotnet\BridgeDotNetWorker\BridgeDotNetWorker.csproj -c Release
```

### Running the worker

```powershell
dotnet run --project dotnet\BridgeDotNetWorker --configuration Release -- --pipe BridgeT4Pipe
```

Or after publishing:

```powershell
.\dotnet\BridgeDotNetWorker\bin\Release\net8.0\BridgeDotNetWorker.exe --pipe BridgeT4Pipe
```

### Connector selection

| Value | Behaviour |
|-------|-----------|
| `STUB` (default) | Returns canned "OK …" responses; no network calls. Safe for CI and development. |
| `REAL` | Connects to the T4 simulator. Requires SDK and credentials (see below). |

Set via `BRIDGE_CONNECTOR` env var **or** the `connector` key in `config/bridge.json`.

### Enabling the Real T4 Connector

1. Obtain the **T4.Api** NuGet package from CTS Futures.  
   Add it to the project or drop it in a local NuGet feed.

2. Build with the `T4SDK` property:
   ```powershell
   dotnet build dotnet\BridgeDotNetWorker\BridgeDotNetWorker.csproj -c Release /p:T4SDK=true
   ```
   This enables the `REAL_T4_SDK` compile-time constant and references `T4.Api`.

3. Set credentials via environment variables (never in config files):
   ```powershell
   $env:BRIDGE_CONNECTOR = "REAL"
   $env:T4_USERNAME      = "your-sim-username"
   $env:T4_PASSWORD      = "your-sim-password"
   $env:T4_LICENSE_KEY   = "your-license-key"   # if required
   ```

4. Run the smoke test (see below).

> If you request `REAL` but did not build with `/p:T4SDK=true`, the connector returns  
> `ERROR REAL connector not available in this build …` immediately. This is intentional fail-fast behaviour.

---

## Smoke Test (Local Windows)

`scripts/smoke-test.ps1` builds the worker, starts it, connects to the pipe, sends
`PING` → `CONNECT` → optional `PLACE`, then tears down the worker.

```powershell
# Minimal (uses STUB connector, no credentials needed)
.\scripts\smoke-test.ps1

# Custom pipe name and config
.\scripts\smoke-test.ps1 -PipeName MyPipe -ConfigPath .\config\bridge.json

# Include a PLACE request
.\scripts\smoke-test.ps1 -PlaceRequest "PLACE ESZ4 BUY 1 4500.00 LIMIT"
```

The script exits with code `0` on success and non-zero on failure.

---

## Deploying the DLL to TradeStation

1. Build in **Release|x64**.
2. Copy `x64\Release\BridgeDLL.dll` to a folder on the system `PATH`, or directly into the TradeStation installation directory.
3. See [TradeStation_EasyLanguage_Usage.md](TradeStation_EasyLanguage_Usage.md) for calling templates.

---

## GitHub Actions CI

The workflow `.github/workflows/windows-ci.yml` automatically:

1. Checks out the repository.
2. Builds the C++ solution via `scripts/build-windows.ps1`.
3. Builds `BridgeDotNetWorker` with `dotnet build` (default/stub mode, no T4.Api needed).
4. Runs `BridgeCoreTests.exe` and fails the workflow if any test fails.
5. Uploads artifacts: `BridgeDLL.dll`, the .NET worker binaries, docs, example config, and log files.
