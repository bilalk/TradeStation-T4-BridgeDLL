#pragma once
#include "IBrokerAdapter.h"

namespace Bridge {

// Stub: .NET adapter (not implemented in this release; always returns RC_NOT_CONNECTED).
class DotNetAdapterStub : public IBrokerAdapter {
public:
    bool IsConnected() const noexcept override { return false; }
    int  Execute(const OrderRequest&) override;
};

} // namespace Bridge
