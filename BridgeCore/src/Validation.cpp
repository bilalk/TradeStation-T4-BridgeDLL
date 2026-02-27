#include "OrderRequest.h"
#include "ResultCodes.h"
#include <string>
#include <algorithm>
#include <cctype>

// ---------------------------------------------------------------------------
// Input validation for order parameters.
// ---------------------------------------------------------------------------

static std::string ToUpper(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(),
        [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return r;
}

// Validate and normalise an OrderRequest in-place.
// Returns RC_SUCCESS or RC_INVALID_PARAM.
int ValidateOrder(OrderRequest& req) {
    if (req.symbol.empty())  return RC_INVALID_PARAM;
    if (req.account.empty()) return RC_INVALID_PARAM;
    if (req.qty <= 0.0)      return RC_INVALID_PARAM;

    req.side = ToUpper(req.side);
    if (req.side != "BUY" && req.side != "SELL") return RC_INVALID_PARAM;

    req.type = ToUpper(req.type);
    if (req.type != "MARKET" && req.type != "LIMIT") return RC_INVALID_PARAM;

    if (req.type == "LIMIT" && req.price <= 0.0) return RC_INVALID_PARAM;

    return RC_SUCCESS;
}
