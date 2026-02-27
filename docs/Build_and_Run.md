# Build and Run Guide

## Prerequisites

- **Windows 10/11 x64**
- **Visual Studio 2022** (Community, Professional, or Enterprise) with the "Desktop development with C++" workload installed
- **Windows SDK 10.0** (installed with the workload above)
- **.NET 8 SDK** (for the BridgeDotNetWorker IPC worker)

---

## Building in Visual Studio 2022

1. Open Visual Studio 2022.
2. Choose **File → Open → Project/Solution**.
3. Navigate to the repository root and open `TradeStation-T4-BridgeDLL.sln`.
4. In the **Solution Configurations** toolbar, select **Release** and **x64**.
5. Press **Ctrl+Shift+B** (or **Build → Build Solution**).

Then build the .NET worker separately:

```powershell
dotnet build dotnet\BridgeDotNetWorker\BridgeDotNetWorker.csproj -c Release
```

### Output Files

After a successful Release build:

| File | Location |
|------|----------|
| `BridgeCore.lib` | `x64\Release\BridgeCore.lib` |
| `BridgeDLL.dll` | `x64\Release\BridgeDLL.dll` |
| `BridgeDLL.lib` | `x64\Release\BridgeDLL.lib` |
| `BridgeCoreTests.exe` | `x64\Release\BridgeCoreTests.exe` |
| `BridgeDotNetWorker.exe` | `dotnet\BridgeDotNetWorker\bin\Release\net8.0-windows\win-x64\` |

---

## Building from the Command Line

Use MSBuild (from a Visual Studio 2022 Developer PowerShell):

```powershell
msbuild TradeStation-T4-BridgeDLL.sln /m /p:Configuration=Release /p:Platform=x64
dotnet build dotnet\BridgeDotNetWorker\BridgeDotNetWorker.csproj -c Release
```

---

## Running Unit Tests

After building, run the unit test executable:

```powershell
.\x64\Release\BridgeCoreTests.exe
```

A passing run prints assertion results and exits with code `0`. Any failure prints `FAIL` and exits with a non-zero code.

---

## Configuration

Copy `config/bridge.example.json` to `config/bridge.json` and edit as needed:

```json
{
  "adapterType": "DOTNET",
  "dotnetWorkerPath": "",
  "pipeName": "\\\\.\\pipe\\BridgeDotNetWorker",
  "t4Host": "uhfix-sim.t4login.com",
  "t4Port": 10443,
  "t4User": "YOUR_T4_USERNAME",
  "t4Password": "YOUR_T4_PASSWORD",
  "t4License": "YOUR_T4_LICENSE_GUID",
  "logPath": "bridge.log"
}
```

- **adapterType**: `MOCK` (default – always succeeds, no connectivity) or `DOTNET` (uses the .NET worker for T4 connectivity).
- **dotnetWorkerPath**: Path to `BridgeDotNetWorker.exe`. If empty, defaults to the `dotnet\` subdirectory next to the DLL.
- **pipeName**: Named pipe for IPC between the DLL and the .NET worker.
- **t4Host** / **t4Port**: T4 simulator endpoint.
- **t4User** / **t4Password** / **t4License**: T4 credentials (never commit these – use `config/bridge.json` which is gitignored, or set via environment variables).
- **logPath**: Path to the log file.

All settings can also be overridden via environment variables:
`BRIDGE_ADAPTER_TYPE`, `BRIDGE_DOTNET_WORKER_PATH`, `BRIDGE_PIPE_NAME`,
`BRIDGE_T4_HOST`, `BRIDGE_T4_PORT`, `BRIDGE_T4_USER`, `BRIDGE_T4_PASSWORD`,
`BRIDGE_T4_LICENSE`, `BRIDGE_LOG_PATH`.

If `config/bridge.json` is not found, the engine uses built-in defaults (MOCK adapter, `bridge.log`).

---

## Deploying the DLL to TradeStation

1. Build in **Release|x64**.
2. Copy `x64\Release\BridgeDLL.dll` to a folder on the system `PATH`, or directly into the TradeStation installation directory.
3. For DOTNET mode, also deploy the `BridgeDotNetWorker.exe` and its runtime files.
4. See [TradeStation_EasyLanguage_Usage.md](TradeStation_EasyLanguage_Usage.md) for calling templates.

---

## GitHub Actions CI

The workflow `.github/workflows/windows-ci.yml` automatically:

1. Checks out the repository.
2. Builds the C++ solution via MSBuild (`Release|x64`).
3. Builds the .NET worker via `dotnet build`.
4. Creates a minimal `config/bridge.json` for testing (no credentials).
5. Runs `BridgeCoreTests.exe` and fails the workflow if any test fails.
6. Optionally runs a real T4 connectivity smoke test (only when secrets are configured).
7. Uploads build artifacts.
