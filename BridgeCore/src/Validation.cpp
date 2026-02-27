#include "Validation.h"
#include "Types.h"
#include <algorithm>
#include <cctype>
#include <string>

namespace Bridge {

static std::string ToUpper(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(),
        [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
    return r;
}

Command ParseCommand(const std::string& s) noexcept {
    std::string u = ToUpper(s);
    if (u == "PLACE")            return Command::PLACE;
    if (u == "CANCEL")           return Command::CANCEL;
    if (u == "CANCELALLORDERS")  return Command::CANCELALLORDERS;
    if (u == "CHANGE")           return Command::CHANGE;
    if (u == "CLOSEPOSITION")    return Command::CLOSEPOSITION;
    if (u == "CLOSESTRATEGY")    return Command::CLOSESTRATEGY;
    if (u == "FLATTENEVERYTHING")return Command::FLATTENEVERYTHING;
    if (u == "REVERSEPOSITION")  return Command::REVERSEPOSITION;
    return Command::UNKNOWN;
}

Action ParseAction(const std::string& s) noexcept {
    std::string u = ToUpper(s);
    if (u == "BUY")  return Action::BUY;
    if (u == "SELL") return Action::SELL;
    return Action::UNKNOWN;
}

OrderType ParseOrderType(const std::string& s) noexcept {
    std::string u = ToUpper(s);
    if (u == "MARKET")     return OrderType::MARKET;
    if (u == "LIMIT")      return OrderType::LIMIT;
    if (u == "STOPMARKET") return OrderType::STOPMARKET;
    if (u == "STOPLIMIT")  return OrderType::STOPLIMIT;
    return OrderType::UNKNOWN;
}

TimeInForce ParseTimeInForce(const std::string& s) noexcept {
    std::string u = ToUpper(s);
    if (u == "DAY") return TimeInForce::DAY;
    if (u == "GTC") return TimeInForce::GTC;
    return TimeInForce::UNKNOWN;
}

int ValidateRequest(const OrderRequest& req) noexcept {
    if (req.command == Command::UNKNOWN)
        return RC_INVALID_CMD;

    // Commands that need account + instrument
    bool needsInstrument = (req.command == Command::PLACE      ||
                            req.command == Command::CANCEL     ||
                            req.command == Command::CHANGE     ||
                            req.command == Command::CLOSEPOSITION ||
                            req.command == Command::CLOSESTRATEGY ||
                            req.command == Command::REVERSEPOSITION);

    if (needsInstrument) {
        if (req.account.empty() || req.instrument.empty())
            return RC_INVALID_PARAM;
    }

    if (req.command == Command::PLACE || req.command == Command::CHANGE) {
        if (req.action    == Action::UNKNOWN)    return RC_INVALID_PARAM;
        if (req.quantity  <= 0)                  return RC_INVALID_PARAM;
        if (req.orderType == OrderType::UNKNOWN) return RC_INVALID_PARAM;
        if (req.timeInForce == TimeInForce::UNKNOWN) return RC_INVALID_PARAM;

        if (req.orderType == OrderType::LIMIT || req.orderType == OrderType::STOPLIMIT) {
            if (req.limitPrice <= 0.0)
                return RC_INVALID_PARAM;
        }
        if (req.orderType == OrderType::STOPMARKET || req.orderType == OrderType::STOPLIMIT) {
            if (req.stopPrice <= 0.0)
                return RC_INVALID_PARAM;
        }
    }

    return RC_SUCCESS;
}

} // namespace Bridge
