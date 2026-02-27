#pragma once
#include "Types.h"
#include <string>

namespace Bridge {

// Parse pipe-delimited payload of the form:
//   command=PLACE|account=ACC1|instrument=ES|action=BUY|quantity=1|
//   orderType=MARKET|limitPrice=0|stopPrice=0|timeInForce=DAY
// Returns RC_SUCCESS or a negative error code.
int ParsePayload(const std::string& payload, OrderRequest& out) noexcept;

// Build an OrderRequest from individual wide-string parameters.
int BuildRequest(const wchar_t* command,
                 const wchar_t* account,
                 const wchar_t* instrument,
                 const wchar_t* action,
                 int            quantity,
                 const wchar_t* orderType,
                 double         limitPrice,
                 double         stopPrice,
                 const wchar_t* timeInForce,
                 OrderRequest&  out) noexcept;

// Narrow-string variant.
int BuildRequest(const char* command,
                 const char* account,
                 const char* instrument,
                 const char* action,
                 int         quantity,
                 const char* orderType,
                 double      limitPrice,
                 double      stopPrice,
                 const char* timeInForce,
                 OrderRequest& out) noexcept;

} // namespace Bridge
