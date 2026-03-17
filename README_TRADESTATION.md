# TradeStation EasyLanguage Integration Guide

This guide explains how to call `BridgeTS.dll` from a TradeStation 10 EasyLanguage strategy.

## Prerequisites

- Copy `Win32\Release\BridgeTS.dll` to `C:\TradeBridge\BridgeTS.dll` (or any fixed path you prefer).
- The DLL is **32-bit (x86)** — required by TradeStation EasyLanguage's DLL call mechanism.

---

## EasyLanguage DLL Declaration

Place this at the top of your EasyLanguage strategy (above `Begin`):

```pascal
{--- Place this at the top of your EasyLanguage strategy ---}
DefineDLLFunc: "C:\TradeBridge\BridgeTS.dll",
    INT, "PLACE_ORDER",
    LPSTR, LPSTR, LPSTR, LPSTR, INT, LPSTR, DOUBLE, DOUBLE, LPSTR;
```

---

## Parameter Reference

| # | EasyLanguage type | C++ parameter | Description |
|---|---|---|---|
| 1 | `LPSTR` | `command` | Order command (see values below) |
| 2 | `LPSTR` | `account` | Account identifier string |
| 3 | `LPSTR` | `instrument` | Instrument symbol, e.g. `"ESH26"` (passed through directly, no mapping) |
| 4 | `LPSTR` | `action` | `"BUY"` or `"SELL"` |
| 5 | `INT` | `quantity` | Number of contracts |
| 6 | `LPSTR` | `orderType` | Order type (see values below) |
| 7 | `DOUBLE` | `limitPrice` | Limit price; pass `0.0` for market orders |
| 8 | `DOUBLE` | `stopPrice` | Stop price; pass `0.0` if not applicable |
| 9 | `LPSTR` | `timeInForce` | Time-in-force (see values below) |

### `command` values

| Value | Meaning |
|---|---|
| `"PLACE"` | Submit a new order |
| `"CANCEL"` | Cancel a specific order |
| `"CANCELALLORDERS"` | Cancel all open orders |
| `"CHANGE"` | Modify an existing order |
| `"CLOSEPOSITION"` | Close an open position |
| `"CLOSESTRATEGY"` | Close all positions opened by this strategy |
| `"FLATTENEVERYTHING"` | Flatten all positions on the account |
| `"REVERSEPOSITION"` | Reverse the current position |

### `orderType` values

| Value | Meaning |
|---|---|
| `"MARKET"` | Market order |
| `"LIMIT"` | Limit order |
| `"STOP"` | Stop order |
| `"STOPLIMIT"` | Stop-limit order |

### `timeInForce` values

| Value | Meaning |
|---|---|
| `"DAY"` | Day order (expires at end of session) |
| `"GTC"` | Good-till-cancelled |
| `"IOC"` | Immediate-or-cancel |

---

## Return Codes

| Code | Constant | Meaning |
|---|---|---|
| `0` | `RC_SUCCESS` | Order accepted successfully |
| `1` | `RC_INVALID_PARAM` | One or more parameters are invalid |
| `2` | `RC_NOT_CONNECTED` | Bridge is not connected to the broker |
| `-1` | `RC_INTERNAL_ERR` | Internal error (check `logs\bridge.log`) |

---

## Example Strategy Snippet

```pascal
{--- Declarations ---}
Vars:
    rc(0);

DefineDLLFunc: "C:\TradeBridge\BridgeTS.dll",
    INT, "PLACE_ORDER",
    LPSTR, LPSTR, LPSTR, LPSTR, INT, LPSTR, DOUBLE, DOUBLE, LPSTR;

{--- Place a buy market order for 1 ES contract ---}
rc = PLACE_ORDER(
    "PLACE",         { command     }
    "MYACCOUNT",     { account     }
    "ESH26",         { instrument  }
    "BUY",           { action      }
    1,               { quantity    }
    "MARKET",        { orderType   }
    0.0,             { limitPrice  }
    0.0,             { stopPrice   }
    "DAY"            { timeInForce }
);

if rc <> 0 then
    Print("PLACE_ORDER failed, rc=", rc);
```

---

## Symbol Verification (Win32 Build)

To confirm the DLL exports `PLACE_ORDER` without stdcall decoration (required by EasyLanguage), run from a Visual Studio Developer Command Prompt:

```
dumpbin /EXPORTS C:\TradeBridge\BridgeTS.dll
```

You should see `PLACE_ORDER` listed — **not** `_PLACE_ORDER@44`. The `.DEF` file strips the decoration so EasyLanguage can find the symbol by its plain name.

---

## Troubleshooting

| Error | Likely Cause | Fix |
|---|---|---|
| "Calling a user DLL function" | DLL not found, wrong bitness, or decorated export name | Confirm the DLL is 32-bit and that `dumpbin` shows `PLACE_ORDER` (not `_PLACE_ORDER@44`) |
| `rc = 1` (INVALID_PARAM) | Bad parameter value | Check command/action/orderType/timeInForce spelling |
| `rc = 2` (NOT_CONNECTED) | BridgeDotNetWorker not running | Start `BridgeDotNetWorker.exe` before running the strategy |
| `rc = -1` (INTERNAL_ERR) | Unhandled exception | Check `logs\bridge.log` for details |
