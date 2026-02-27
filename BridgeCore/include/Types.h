#pragma once
#include <string>

namespace Bridge {

// Return codes
constexpr int RC_SUCCESS        =  0;
constexpr int RC_INVALID_CMD    = -1;
constexpr int RC_INVALID_PARAM  = -2;
constexpr int RC_NOT_CONNECTED  = -3;
constexpr int RC_INTERNAL_ERR   = -4;
constexpr int RC_CONFIG_ERR     = -6;

enum class Command {
    PLACE,
    CANCEL,
    CANCELALLORDERS,
    CHANGE,
    CLOSEPOSITION,
    CLOSESTRATEGY,
    FLATTENEVERYTHING,
    REVERSEPOSITION,
    UNKNOWN
};

enum class Action {
    BUY,
    SELL,
    UNKNOWN
};

enum class OrderType {
    MARKET,
    LIMIT,
    STOPMARKET,
    STOPLIMIT,
    UNKNOWN
};

enum class TimeInForce {
    DAY,
    GTC,
    UNKNOWN
};

struct OrderRequest {
    Command     command     = Command::UNKNOWN;
    std::string account;
    std::string instrument;
    Action      action      = Action::UNKNOWN;
    int         quantity    = 0;
    OrderType   orderType   = OrderType::UNKNOWN;
    double      limitPrice  = 0.0;
    double      stopPrice   = 0.0;
    TimeInForce timeInForce = TimeInForce::UNKNOWN;
};

} // namespace Bridge
