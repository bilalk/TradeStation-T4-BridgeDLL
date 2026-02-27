# .NET Worker and IPC Architecture

This document describes the IPC bridge between the native C++ BridgeDLL and
the .NET 8 worker process (`BridgeDotNetWorker`).

---

## Architecture Overview

```
TradeStation (or any caller)
        │
        │ Win32 DLL call (PLACE_ORDER_A / PLACE_ORDER_W / …)
        ▼
  BridgeDLL.dll  (C++, x64)
        │
        │ Named Pipe IPC  \\.\pipe\BridgeDotNetWorker
        ▼
  BridgeDotNetWorker.exe  (.NET 8, spawned on first use)
        │
        │ T4 / FIX connectivity (stub → real T4.Api NuGet)
        ▼
  T4 Simulator  (uhfix-sim.t4login.com:10443, FIX 4.2 / TLS)
```

---

## IPC Protocol

Line-oriented UTF-8 text over a named pipe (`\\.\pipe\BridgeDotNetWorker`).

| Request line                        | Response line              |
|-------------------------------------|----------------------------|
| `PING`                              | `PONG`                     |
| `CONNECT`                           | `OK …` or `ERROR …`        |
| `PLACE symbol=… account=… qty=… price=… side=… type=…` | `OK …` or `ERROR …` |
| `SHUTDOWN`                          | `OK shutting down`         |

Each line is terminated by `\n`.  The server responds with exactly one line
per request.

---

## Setting up `config/bridge.json` locally

1. Copy the example file:

   ```sh
   cp config/bridge.example.json config/bridge.json
   ```

2. Edit `config/bridge.json` and fill in your T4 simulator credentials:

   ```json
   {
     "adapterType": "DOTNET",
     "pipeName": "\\\\.\\pipe\\BridgeDotNetWorker",
     "t4Host": "uhfix-sim.t4login.com",
     "t4Port": 10443,
     "t4User": "<your T4 username>",
     "t4Password": "<your T4 password>",
     "t4License": "<your T4 license GUID>",
     "logPath": "bridge.log"
   }
   ```

   > **SECURITY**: `config/bridge.json` is listed in `.gitignore` and must
   > **never** be committed.

---

## Switching `adapterType` to DOTNET

The `adapterType` key in `config/bridge.json` (or the `BRIDGE_ADAPTER_TYPE`
environment variable) controls which adapter the DLL uses:

| Value    | Behaviour                                      |
|----------|------------------------------------------------|
| `MOCK`   | Always returns success; no network access.     |
| `DOTNET` | Spawns `BridgeDotNetWorker.exe` and uses IPC.  |
| `FIX`    | (future) Direct FIX 4.2 native implementation.|

---

## Building

### Prerequisites

- Windows 10/11 or Windows Server 2022
- Visual Studio 2022 (Community or higher) with **Desktop development with C++** workload
- .NET 8 SDK (`winget install Microsoft.DotNet.SDK.8` or https://dotnet.microsoft.com)

### Build C++ solution

```powershell
# From repo root
.\scripts\build-windows.ps1 -Configuration Release -Platform x64
```

Or open `TradeStation-T4-BridgeDLL.sln` in Visual Studio and build.

### Build .NET worker

```powershell
dotnet build dotnet\BridgeDotNetWorker\BridgeDotNetWorker.csproj -c Release
```

To publish as a self-contained single file:

```powershell
dotnet publish dotnet\BridgeDotNetWorker\BridgeDotNetWorker.csproj `
  -c Release -r win-x64 --self-contained true `
  -p:PublishSingleFile=true `
  -o x64\Release\dotnet\BridgeDotNetWorker
```

---

## Running the worker manually

```powershell
# Default pipe name
.\x64\Release\dotnet\BridgeDotNetWorker\BridgeDotNetWorker.exe

# Custom pipe name
.\x64\Release\dotnet\BridgeDotNetWorker\BridgeDotNetWorker.exe --pipe "\\.\pipe\MyPipe"

# With explicit config file
.\x64\Release\dotnet\BridgeDotNetWorker\BridgeDotNetWorker.exe --config "C:\path\to\bridge.json"
```

---

## Running the integration test locally

1. Build both the C++ solution and the .NET worker (see above).
2. Create `config/bridge.json` with valid simulator credentials.
3. Run the test executable:

   ```powershell
   .\x64\Release\BridgeCoreTests.exe
   ```

   Expected output (with credentials):
   ```
   === BridgeCoreTests ===
   TestConfigEnvOverride done
   TestValidation done
   TestMockAdapter done
   TestDotNetAdapterPing done
   TestDotNetAdapterConnect done

   Results: N passed, 0 failed.
   ```

4. To test CONNECT with real T4 credentials:

   Set `adapterType=DOTNET` in `config/bridge.json`, then run
   `BridgeCoreTests.exe`.  The `TestDotNetAdapterConnect` test sends a
   `CONNECT` command to the worker which calls `IT4Connector.ConnectAsync`.

   With the stub connector the response is:
   - `OK (stub) connected to uhfix-sim.t4login.com`  (when credentials are present)
   - `ERROR no credentials configured`               (when credentials are absent)

   Once the `T4.Api` NuGet package is wired in, replace `StubT4Connector`
   with `RealT4Connector` in `Program.cs`.

---

## Real T4 SDK integration (next steps)

The `StubT4Connector` in `T4Connector.cs` documents what a real implementation
requires:

1. Add NuGet package reference in `BridgeDotNetWorker.csproj`:

   ```xml
   <PackageReference Include="T4.Api" Version="6.*" />
   ```

   Package source: https://www.nuget.org/profiles/CTSFutures

2. Implement `RealT4Connector`:

   ```csharp
   // Pseudocode – actual API may differ; consult CTS documentation.
   using CTS.T4.API;

   public class RealT4Connector : IT4Connector
   {
       private Host? _host;

       public async Task<string> ConnectAsync(WorkerConfig cfg, CancellationToken ct)
       {
           _host = new Host(cfg.T4Host, cfg.T4Port);
           var tcs = new TaskCompletionSource<string>();
           _host.OnLoginComplete += (s, e) => tcs.TrySetResult(e.Success ? "OK" : "ERROR " + e.Message);
           _host.Login(cfg.T4User, cfg.T4Password, cfg.T4License);
           return await tcs.Task;
       }
       // ...
   }
   ```

3. Replace `new StubT4Connector()` with `new RealT4Connector()` in `Program.cs`.

---

## CI/CD (GitHub Actions)

The workflow `.github/workflows/windows-ci.yml`:

- Builds the Visual Studio solution (`msbuild`, Release|x64).
- Builds the .NET worker (`dotnet build`).
- Runs `BridgeCoreTests.exe`.
- Uploads artifacts (DLL + worker exe).
- Does **not** run real T4 connectivity unless `BRIDGE_T4_USER` is set as a
  GitHub Actions secret.

To enable real connectivity in CI, add the following repository secrets:

| Secret name          | Value                        |
|----------------------|------------------------------|
| `BRIDGE_T4_USER`     | T4 username                  |
| `BRIDGE_T4_PASSWORD` | T4 password                  |
| `BRIDGE_T4_LICENSE`  | T4 license GUID              |
