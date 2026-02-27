#pragma once
#include <string>

enum class OrderCmd { PLACE, CANCEL, MODIFY };

struct OrderRequest {
    OrderCmd    cmd     = OrderCmd::PLACE;
    std::string symbol;
    std::string account;
    double      qty     = 0.0;
    double      price   = 0.0;
    std::string side;   // "BUY" or "SELL"
    std::string type;   // "MARKET" or "LIMIT"
};
