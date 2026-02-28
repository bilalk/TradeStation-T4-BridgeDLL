{ ---- TradeStation-T4 Bridge Test Strategy ---- }
{ Drop BridgeTS.dll into C:\Program Files\TradeStation 10.0\Program }

DefineDLLFunc: "BridgeTS.dll",
    INT, "PLACE_ORDER",
    LPSTR, LPSTR, LPSTR, LPSTR,
    INT,
    LPSTR, DOUBLE, DOUBLE, LPSTR;

vars:
    DLLResult(0);

if LastBarOnChart then begin
    DLLResult = PLACE_ORDER(
        "PLACE",
        "ACC001",
        "ESH26",
        "BUY",
        1,
        "MARKET",
        0.0,
        0.0,
        "DAY"
    );

    if DLLResult = 0 then
        Print("Order placed successfully")
    else
        Print("Order failed, code=", DLLResult);
end;
