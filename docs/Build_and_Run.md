# Build and Run Guide

## Prerequisites

- **Windows 10/11 x64**
- **Visual Studio 2022** (Community, Professional, or Enterprise) with the "Desktop development with C++" workload installed
- **Windows SDK 10.0** (installed with the workload above)

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

Copy `config/bridge.example.json` to `config/bridge.json` and edit as needed:

```json
{
  "adapterType": "MOCK",
  "logFilePath": "logs/bridge.log",
  "logToConsole": false
}
```

- **adapterType**: `MOCK` (default), `FIX` (stub, not yet implemented), `DOTNET` (stub, not yet implemented).
- **logFilePath**: Path to the log file. The directory is created automatically.
- **logToConsole**: Set to `true` to also print log lines to stdout.

If `config/bridge.json` is not found, the engine uses built-in defaults (MOCK adapter, `logs/bridge.log`).

---

## Deploying the DLL to TradeStation

1. Build in **Release|x64**.
2. Copy `x64\Release\BridgeDLL.dll` to a folder on the system `PATH`, or directly into the TradeStation installation directory.
3. See [TradeStation_EasyLanguage_Usage.md](TradeStation_EasyLanguage_Usage.md) for calling templates.

---

## GitHub Actions CI

The workflow `.github/workflows/windows-ci.yml` automatically:

1. Checks out the repository.
2. Builds the solution via `scripts/build-windows.ps1`.
3. Runs `BridgeCoreTests.exe` and fails the workflow if any test fails.
4. Uploads artifacts: `BridgeDLL.dll`, docs, example config, and log files.
