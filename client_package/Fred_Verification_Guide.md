# Fred Verification Guide — BridgeTS.dll (TradeStation 10)

**Audience:** Non-programmer quant validating the TradeStation-only deliverable.  
**Scope:** `BridgeTS.dll` only — no T4 credentials, no .NET worker, no FIX connection required.

---

## What You Are Verifying

`BridgeTS.dll` is the C++ DLL that TradeStation 10 calls when an EasyLanguage strategy sends
a trade command (PLACE, CANCEL, etc.).  In verification mode the DLL runs its built-in **MOCK**
adapter, which:

- validates every incoming parameter,
- logs every call to `logs\bridge.log` (relative to the folder containing the DLL), and
- returns **return code 0** ("Order placed successfully") for any valid PLACE request.

No live T4 brokerage connection is required.

---

## Step 1 — Install Visual Studio 2022

1. Download **Visual Studio 2022 Community** (free) from  
   <https://visualstudio.microsoft.com/vs/community/>
2. In the installer, select the workload **"Desktop development with C++"**.  
   This pulls in the MSVC compiler, Windows SDK 10.0, and MSBuild.
3. Complete the installation (≈ 6–8 GB, may take 20–40 minutes).

---

## Step 2 — Open the Solution

1. Clone or unzip the repository so you have a local copy.
2. Open Visual Studio 2022.
3. Choose **File → Open → Project/Solution**.
4. Navigate to the repository root and open **`TradeStation-T4-BridgeDLL.sln`**.

---

## Step 3 — Build Release x64

1. In the **Solution Configurations** toolbar (top of the IDE), select **Release**.
2. In the adjacent **Solution Platforms** toolbar, select **x64**.
3. Press **Ctrl+Shift+B** (or **Build → Build Solution**).
4. Wait for the Output pane to show `========== Build: X succeeded, 0 failed ==========`.

### Output file you need

```
x64\Release\BridgeTS.dll
```

> If the build fails, verify that the "Desktop development with C++" workload is installed
> and that you chose Release | x64.

---

## Step 4 — Copy BridgeTS.dll to TradeStation

1. Open Windows Explorer and navigate to the build output:  
   `<repo root>\x64\Release\`
2. Copy **`BridgeTS.dll`** to the TradeStation 10 program folder:  
   `C:\Program Files\TradeStation 10.0\Program\`  
   (Accept the UAC elevation prompt if asked.)
3. *(Optional but recommended)* Also create a `logs` sub-folder inside the TradeStation
   program folder so the DLL can write its log file there:  
   `C:\Program Files\TradeStation 10.0\Program\logs\`

---

## Step 5 — Add the EasyLanguage Test Strategy

1. Open TradeStation 10.
2. Open the **EasyLanguage Editor** (**Tools → EasyLanguage Editor** or press **F3**).
3. Create a new **Strategy** (not a function or indicator).
4. Delete any default code and paste the entire contents of  
   **`client_package\EasyLanguage_TestStrategy.txt`** (included in this package).
5. Compile the strategy (**Ctrl+Alt+B** or the Compile toolbar button).  
   It should compile with 0 errors and 0 warnings.
6. Attach the strategy to any chart (daily or intraday, any symbol).

---

## Step 6 — Run the Strategy and Confirm Output

1. With the strategy attached, TradeStation will call `PLACE_ORDER` on the last bar of the chart.
2. Open the **Strategy Performance Report** tab or the **Print Log** (`View → Print Log`)
   to see the output.
3. You should see:

   ```
   Order placed successfully (rc=0)
   ```

   If you see a negative return code instead, check that `BridgeTS.dll` is in the correct
   folder and that it is the 64-bit Release build.

---

## Step 7 — Confirm the Log File

1. Navigate to the folder where `BridgeTS.dll` lives:  
   `C:\Program Files\TradeStation 10.0\Program\`
2. Open the `logs` sub-folder.
3. Open **`bridge.log`** in Notepad.
4. You should see timestamped entries like:

   ```
   [INFO]  [REQ-0001] PLACE_ORDER called command=PLACE account=ACC001 instrument=ESH26 ...
   [INFO]  [REQ-0001] Validation: OK
   [INFO]  Execute succeeded: command=0
   ```

   This confirms the DLL received the parameters, validated them, and executed successfully.

---

## Troubleshooting

| Symptom | Likely cause | Fix |
|---------|-------------|-----|
| Strategy fails to compile in EasyLanguage | DLL not found or wrong path | Verify DLL is in `C:\Program Files\TradeStation 10.0\Program\` |
| Return code = –2 | Invalid parameter (null account, zero quantity, etc.) | Check the EasyLanguage call matches the template |
| Return code = –1 | Unknown command string | Ensure `"PLACE"` is spelled correctly (case-insensitive) |
| No `bridge.log` created | `logs\` folder missing | Create the `logs` sub-folder next to `BridgeTS.dll` |
| DLL loads but crashes | Wrong architecture (32-bit vs 64-bit) | Rebuild in Release\|x64 and re-copy |

---

## Return Code Reference

| Code | Constant | Meaning |
|------|----------|---------|
| 0 | `RC_SUCCESS` | Order accepted by mock adapter |
| –1 | `RC_INVALID_CMD` | Unknown command string |
| –2 | `RC_INVALID_PARAM` | Missing or invalid parameter |
| –3 | `RC_NOT_CONNECTED` | Adapter not connected (should not occur with MOCK) |
| –4 | `RC_INTERNAL_ERR` | Unexpected internal exception |
| –6 | `RC_CONFIG_ERR` | Config file parse error (falls back to MOCK automatically) |

---

## What Is Out of Scope for This Deliverable

- **T4 live/simulation connectivity** — requires separate FIX credentials and the
  `BridgeDotNetWorker` .NET process; not needed for this verification.
- **`BridgeDLL.dll`** — an older DLL variant; use `BridgeTS.dll` for all TradeStation 10 work.
- **`BridgeDotNetWorker.exe`** — the optional .NET 8 side-car process for real T4 routing;
  not required when using the MOCK adapter.
