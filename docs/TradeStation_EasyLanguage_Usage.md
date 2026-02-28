# TradeStation EasyLanguage Usage Guide

This guide shows how to call the `BridgeDLL.dll` / `BridgeTS.dll` exported functions from TradeStation's EasyLanguage.

## Prerequisite

Place the DLL (and its dependencies) in your TradeStation installation folder or a directory on the system `PATH`.

---

## 0. Simplified ANSI Entry Point: `PLACE_ORDER` (BridgeTS.dll — recommended for TradeStation 10)

Use **`BridgeTS.dll`** for the cleanest single-function interface.  Copy `BridgeTS.dll` to
`C:\Program Files\TradeStation 10.0\Program\`.

### EasyLanguage Declaration

```easylanguage
DefineDLLFunc: "BridgeTS.dll",
    INT, "PLACE_ORDER",
    LPSTR, LPSTR, LPSTR, LPSTR,
    INT,
    LPSTR, DOUBLE, DOUBLE, LPSTR;
```

### EasyLanguage Call Example

```easylanguage
vars:
    DLLResult(0);

if LastBarOnChart then begin
    DLLResult = PLACE_ORDER(
        "PLACE",      { command        }
        "ACC001",     { account        }
        "ESH26",      { instrument     }
        "BUY",        { action         }
        1,            { quantity       }
        "MARKET",     { orderType      }
        0.0,          { limitPrice     }
        0.0,          { stopPrice      }
        "DAY"         { timeInForce    }
    );

    if DLLResult = 0 then
        Print("Order placed successfully")
    else
        Print("Order failed, code=", DLLResult);
end;
```

---

## 1. Multi-Argument Variant (`PLACE_ORDER_W` / `PLACE_ORDER_A`) — BridgeDLL.dll

Use this when each parameter is a separate variable.

### EasyLanguage Declaration

```easylanguage
DefineDLLFunc: "BridgeDLL.dll",
    INT, "PLACE_ORDER_W",
    WSTRING, WSTRING, WSTRING, WSTRING,
    INT,
    WSTRING, DOUBLE, DOUBLE, WSTRING;
```

### EasyLanguage Call Example

```easylanguage
vars:
    DLLResult(0);

DLLResult = PLACE_ORDER_W(
    "PLACE",      { command        }
    "ACC001",     { account        }
    "ES",         { instrument     }
    "BUY",        { action         }
    1,            { quantity       }
    "MARKET",     { orderType      }
    0.0,          { limitPrice     }
    0.0,          { stopPrice      }
    "DAY"         { timeInForce    }
);

if DLLResult <> 0 then
    Print("Order failed, code=", DLLResult);
```

### Supported Values

| Parameter   | Accepted values (case-insensitive)              |
|-------------|-------------------------------------------------|
| command     | `PLACE`, `CANCEL`, `CANCELALLORDERS`, `CHANGE`, `CLOSEPOSITION`, `CLOSESTRATEGY`, `FLATTENEVERYTHING`, `REVERSEPOSITION` |
| action      | `BUY`, `SELL`                                   |
| orderType   | `MARKET`, `LIMIT`, `STOPMARKET`, `STOPLIMIT`    |
| timeInForce | `DAY`, `GTC`                                    |

---

## 2. Single Pipe-Delimited Payload Variant (`PLACE_ORDER_CMD_W` / `PLACE_ORDER_CMD_A`)

Use this when you want to build the full command as one string (useful for strategy-level string construction).

### EasyLanguage Declaration

```easylanguage
DefineDLLFunc: "BridgeDLL.dll",
    INT, "PLACE_ORDER_CMD_W",
    WSTRING;
```

### Payload Format

```
command=<CMD>|account=<ACCT>|instrument=<INSTR>|action=<ACT>|quantity=<QTY>|orderType=<OT>|limitPrice=<LP>|stopPrice=<SP>|timeInForce=<TIF>
```

Keys are **case-insensitive**. The `|` delimiter separates fields.

### EasyLanguage Call Example

```easylanguage
vars:
    DLLResult(0),
    Payload("");

Payload = "command=PLACE|account=ACC001|instrument=ES|action=BUY|" +
          "quantity=1|orderType=MARKET|limitPrice=0|stopPrice=0|timeInForce=DAY";

DLLResult = PLACE_ORDER_CMD_W(Payload);

if DLLResult <> 0 then
    Print("Order failed, code=", DLLResult);
```

---

## Return Codes

| Code | Meaning                           |
|------|-----------------------------------|
|  `0` | Success (order accepted/queued)   |
| `-1` | Invalid command                   |
| `-2` | Invalid parameters                |
| `-3` | Not connected / adapter unavailable |
| `-4` | Internal error                    |
| `-6` | Config error                      |

---

## Default Semantics

| Command            | Behaviour                                                   |
|--------------------|-------------------------------------------------------------|
| `CANCEL`           | Cancels all working orders for (account + instrument)       |
| `CHANGE`           | Cancels all working orders for (account + instrument), then places a new order with the provided params |
| `CLOSESTRATEGY`    | Alias for `FLATTENEVERYTHING` — cancels all working orders  |
| `CLOSEPOSITION`    | Cancels all working orders for (account + instrument)       |
| `FLATTENEVERYTHING`| Cancels all working orders across all instruments           |
| `REVERSEPOSITION`  | Cancels working orders for the position, then places the opposite-side order |
| `CANCELALLORDERS`  | Cancels all working orders for the given account            |
