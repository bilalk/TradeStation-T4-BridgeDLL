#pragma once
#include "Types.h"
#include <string>

namespace Bridge {

class IBrokerAdapter {
public:
    virtual ~IBrokerAdapter() = default;

    virtual bool IsConnected() const noexcept = 0;

    // Execute an order request; returns a Bridge return code.
    virtual int Execute(const OrderRequest& req) = 0;
};

} // namespace Bridge
