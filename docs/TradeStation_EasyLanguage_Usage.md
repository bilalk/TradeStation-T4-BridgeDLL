# TradeStation EasyLanguage Usage Guide

This guide shows how to call the `BridgeDLL.dll` exported functions from TradeStation's EasyLanguage.

## Prerequisite

Place `BridgeDLL.dll` (and its dependencies) in your TradeStation installation folder or a directory on the system `PATH`.

---

## 1. Multi-Argument Variant (`PLACE_ORDER_W` / `PLACE_ORDER_A`)

Use this when each parameter is a separate variable.

### EasyLanguage Declaration

```easylanguage
DefineDLLFunc: "BridgeDLL.dll",
    INT, "PLACE_ORDER_W",
    WSTRING, WSTRING,
    DOUBLE, DOUBLE,
    WSTRING, WSTRING;
```

### EasyLanguage Call Example

```easylanguage
vars:
    DLLResult(0);

DLLResult = PLACE_ORDER_W(
    "ESZ4",       { symbol         }
    "ACC001",     { account        }
    1.0,          { quantity       }
    4500.0,       { price (0 for MARKET) }
    "BUY",        { side           }
    "LIMIT"       { orderType      }
);

if DLLResult <> 0 then
    Print("Order failed, code=", DLLResult);
```

### Parameter Reference

| Parameter | Type   | Description                         |
|-----------|--------|-------------------------------------|
| symbol    | string | Instrument symbol (e.g. `"ESZ4"`)   |
| account   | string | Account identifier                  |
| qty       | double | Order quantity (must be > 0)        |
| price     | double | Limit price (0 for MARKET orders)   |
| side      | string | `"BUY"` or `"SELL"` (case-insensitive) |
| orderType | string | `"MARKET"` or `"LIMIT"` (case-insensitive) |

---

## 2. Single Command String Variant (`PLACE_ORDER_CMD_W` / `PLACE_ORDER_CMD_A`)

Use this when you want to build the full command as one string.

### EasyLanguage Declaration

```easylanguage
DefineDLLFunc: "BridgeDLL.dll",
    INT, "PLACE_ORDER_CMD_W",
    WSTRING;
```

### Command Format

Space-separated fields:

```
SYMBOL ACCOUNT QTY PRICE SIDE TYPE
```

Example: `"ESZ4 MyAcct 1 4500.0 BUY LIMIT"`

### EasyLanguage Call Example

```easylanguage
vars:
    DLLResult(0),
    Payload("");

Payload = "ESZ4 ACC001 1 4500.0 BUY LIMIT";

DLLResult = PLACE_ORDER_CMD_W(Payload);

if DLLResult <> 0 then
    Print("Order failed, code=", DLLResult);
```

---

## Return Codes

| Code | Name              | Meaning                              |
|------|-------------------|--------------------------------------|
| `0`  | `RC_SUCCESS`      | Order accepted / operation succeeded |
| `1`  | `RC_INVALID_PARAM`| Invalid or missing parameters        |
| `2`  | `RC_NOT_CONNECTED`| Adapter not connected                |
| `3`  | `RC_TIMEOUT`      | Operation timed out                  |
| `4`  | `RC_ERROR`        | General error                        |
