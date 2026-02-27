#pragma once
#include "IBrokerAdapter.h"
#include "Config.h"
#include "Types.h"
#include <memory>

namespace Bridge {

class BridgeEngine {
public:
    explicit BridgeEngine(const BridgeConfig& cfg);
    ~BridgeEngine() = default;

    // Execute a fully-populated request.
    int Execute(const OrderRequest& req) noexcept;

    bool IsConnected() const noexcept;

private:
    std::shared_ptr<IBrokerAdapter> m_adapter;
    BridgeConfig                    m_config;
};

// Singleton accessor; initialised once on first call.
BridgeEngine& GetEngine() noexcept;

} // namespace Bridge
