# Fred Verification Guide — BridgeTS.dll

This guide explains how to download the pre-built `BridgeTS.dll` from GitHub Actions
and verify it works inside TradeStation 10 EasyLanguage — **no Visual Studio needed**.

---

## Step 1 — Download the artifact from GitHub Actions

1. Open the repository on GitHub:
   `https://github.com/bilalk/TradeStation-T4-BridgeDLL`
2. Click **Actions** in the top navigation bar.
3. Click the **fred-verification** workflow on the left.
4. Click the most recent successful run (green checkmark).
5. Scroll to the **Artifacts** section at the bottom of the run summary.
6. Click **fred-verification-package** to download the zip file.
7. Extract the zip on your machine — you will find `BridgeTS.dll` inside.

---

## Step 2 — Install the DLL on your TradeStation machine

Copy `BridgeTS.dll` into the TradeStation program directory:

```
C:\Program Files\TradeStation 10.0\Program\BridgeTS.dll
```

> **Tip:** You may need to run File Explorer as Administrator to copy files into
> `C:\Program Files\`.

---

## Step 3 — Import the EasyLanguage template

1. Open TradeStation 10.
2. Open the **EasyLanguage Editor** (menu: **Tools → EasyLanguage Editor**).
3. Create a new **Strategy** document (File → New → Strategy).
4. Paste the contents of `EasyLanguage_Template.eld` (included in the
   artifact zip under `client_package/`) into the editor.
5. Compile the strategy (press **F3** or click **Verify**).
6. Apply the strategy to any chart.

---

## Step 4 — Confirm a successful return code

When the strategy runs on the last bar of the chart it calls:

```
PLACE_ORDER("PLACE", "ACC001", "ESH26", "BUY", 1, "MARKET", 0.0, 0.0, "DAY")
```

In **mock mode** (no live T4 connection), the DLL processes the request internally
and returns `0` (RC_SUCCESS).

You should see in the TradeStation **Print Log** (View → Print Log):

```
Order placed successfully
```

If you see `Order failed, code=<N>`, the return code meanings are:

| Code | Constant        | Meaning                          |
|------|-----------------|----------------------------------|
|  0   | RC_SUCCESS      | Order accepted                   |
| -1   | RC_INVALID_CMD  | Unrecognised command string      |
| -2   | RC_INVALID_PARAM| A required parameter is invalid  |
| -3   | RC_NOT_CONNECTED| Adapter not connected            |
| -4   | RC_INTERNAL_ERR | Unexpected internal error        |
| -6   | RC_CONFIG_ERR   | Configuration file error         |

---

## Step 5 — Check the log file

`BridgeTS.dll` writes a log to:

```
logs\bridge.log
```

relative to the working directory of the process that loaded the DLL.
When loaded by TradeStation, this is typically:

```
C:\Program Files\TradeStation 10.0\Program\logs\bridge.log
```

Each successful PLACE call produces two log lines similar to:

```
[INFO] [REQ-0001] PLACE_ORDER called command=PLACE account=ACC001 ...
[INFO] [REQ-0001] Validation: OK
[INFO] [REQ-0001] Execute returned rc=0
```

---

## Automated CI verification (for reference)

The `fred-verification` GitHub Actions workflow:

1. Checks out the repository on a **Windows Server 2022** runner.
2. Builds the solution in **Release | x64** using Visual Studio 2022 MSBuild.
3. Runs `BridgeTSTests.exe` which dynamically loads `BridgeTS.dll` and calls
   `PLACE_ORDER` for all valid and invalid inputs, printing `[PASS]` / `[FAIL]`.
4. Uploads `BridgeTS.dll` + this guide as the **fred-verification-package** artifact.

No T4 credentials, no named pipes, and no .NET worker are needed for this workflow.
