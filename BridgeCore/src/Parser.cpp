#include "Parser.h"
#include "Validation.h"
#include "Types.h"
#include <string>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <stdexcept>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace Bridge {

static std::string WideToNarrow(const wchar_t* w) {
    if (!w) return {};
#ifdef _WIN32
    int len = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string s(static_cast<size_t>(len - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), len, nullptr, nullptr);
    return s;
#else
    // Fallback for non-Windows compilation (tests on Linux)
    std::string s;
    while (*w) { s += static_cast<char>(*w++); }
    return s;
#endif
}

static std::string Trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

int ParsePayload(const std::string& payload, OrderRequest& out) noexcept {
    try {
        // Split on '|'
        std::istringstream ss(payload);
        std::string token;
        while (std::getline(ss, token, '|')) {
            token = Trim(token);
            if (token.empty()) continue;
            size_t eq = token.find('=');
            if (eq == std::string::npos) continue;
            std::string key = Trim(token.substr(0, eq));
            std::string val = Trim(token.substr(eq + 1));

            // Case-insensitive key match
            std::string ku = key;
            std::transform(ku.begin(), ku.end(), ku.begin(),
                [](unsigned char c){ return static_cast<char>(std::toupper(c)); });

            if      (ku == "COMMAND")     out.command     = ParseCommand(val);
            else if (ku == "ACCOUNT")     out.account     = val;
            else if (ku == "INSTRUMENT")  out.instrument  = val;
            else if (ku == "ACTION")      out.action      = ParseAction(val);
            else if (ku == "QUANTITY")    out.quantity    = std::stoi(val);
            else if (ku == "ORDERTYPE")   out.orderType   = ParseOrderType(val);
            else if (ku == "LIMITPRICE")  out.limitPrice  = std::stod(val);
            else if (ku == "STOPPRICE")   out.stopPrice   = std::stod(val);
            else if (ku == "TIMEINFORCE") out.timeInForce = ParseTimeInForce(val);
        }
        return ValidateRequest(out);
    }
    catch (...) {
        return RC_INVALID_PARAM;
    }
}

int BuildRequest(const wchar_t* command,
                 const wchar_t* account,
                 const wchar_t* instrument,
                 const wchar_t* action,
                 int            quantity,
                 const wchar_t* orderType,
                 double         limitPrice,
                 double         stopPrice,
                 const wchar_t* timeInForce,
                 OrderRequest&  out) noexcept {
    try {
        out.command     = ParseCommand(WideToNarrow(command));
        out.account     = WideToNarrow(account);
        out.instrument  = WideToNarrow(instrument);
        out.action      = ParseAction(WideToNarrow(action));
        out.quantity    = quantity;
        out.orderType   = ParseOrderType(WideToNarrow(orderType));
        out.limitPrice  = limitPrice;
        out.stopPrice   = stopPrice;
        out.timeInForce = ParseTimeInForce(WideToNarrow(timeInForce));
        return ValidateRequest(out);
    }
    catch (...) {
        return RC_INVALID_PARAM;
    }
}

int BuildRequest(const char* command,
                 const char* account,
                 const char* instrument,
                 const char* action,
                 int         quantity,
                 const char* orderType,
                 double      limitPrice,
                 double      stopPrice,
                 const char* timeInForce,
                 OrderRequest& out) noexcept {
    try {
        out.command     = ParseCommand(command     ? command     : "");
        out.account     = account     ? account     : "";
        out.instrument  = instrument  ? instrument  : "";
        out.action      = ParseAction(action       ? action      : "");
        out.quantity    = quantity;
        out.orderType   = ParseOrderType(orderType ? orderType   : "");
        out.limitPrice  = limitPrice;
        out.stopPrice   = stopPrice;
        out.timeInForce = ParseTimeInForce(timeInForce ? timeInForce : "");
        return ValidateRequest(out);
    }
    catch (...) {
        return RC_INVALID_PARAM;
    }
}

} // namespace Bridge
