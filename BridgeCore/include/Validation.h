#pragma once
#include "Types.h"
#include <string>

namespace Bridge {

// Parse a single uppercase token into the corresponding Command enum.
Command    ParseCommand    (const std::string& s) noexcept;
Action     ParseAction     (const std::string& s) noexcept;
OrderType  ParseOrderType  (const std::string& s) noexcept;
TimeInForce ParseTimeInForce(const std::string& s) noexcept;

// Validate a fully-populated OrderRequest.
// Returns RC_SUCCESS (0) or a negative error code.
int ValidateRequest(const OrderRequest& req) noexcept;

} // namespace Bridge
