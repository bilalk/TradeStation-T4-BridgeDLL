#include "BridgeEngine.h"
#include "Validation.h"
#include "Logger.h"
#include "Config.h"
#include "MockAdapter.h"
#include "FixAdapterStub.h"
#include "DotNetAdapterStub.h"
#include <stdexcept>
#include <filesystem>

namespace Bridge {

BridgeEngine::BridgeEngine(const BridgeConfig& cfg)
    : m_config(cfg)
{
    LogInit(cfg.logFilePath, cfg.logToConsole);
    LogInfo("BridgeEngine initialising with adapter=" + cfg.adapterType);

    if (cfg.adapterType == "FIX") {
        m_adapter = std::make_shared<FixAdapterStub>();
    } else if (cfg.adapterType == "DOTNET") {
        m_adapter = std::make_shared<DotNetAdapterStub>();
    } else {
        // Default: MOCK
        m_adapter = std::make_shared<MockAdapter>();
    }
}

int BridgeEngine::Execute(const OrderRequest& req) noexcept {
    try {
        if (!m_adapter || !m_adapter->IsConnected()) {
            LogError("Adapter not connected");
            return RC_NOT_CONNECTED;
        }
        int rc = m_adapter->Execute(req);
        if (rc == RC_SUCCESS)
            LogInfo("Execute succeeded: command=" + std::to_string(static_cast<int>(req.command)));
        else
            LogWarning("Execute returned code=" + std::to_string(rc));
        return rc;
    }
    catch (const std::exception& ex) {
        LogError(std::string("Exception in Execute: ") + ex.what());
        return RC_INTERNAL_ERR;
    }
    catch (...) {
        LogError("Unknown exception in Execute");
        return RC_INTERNAL_ERR;
    }
}

bool BridgeEngine::IsConnected() const noexcept {
    return m_adapter && m_adapter->IsConnected();
}

BridgeEngine& GetEngine() noexcept {
    // Load config once from file (or use defaults)
    static BridgeConfig cfg = []() -> BridgeConfig {
        BridgeConfig c;
        int rc = LoadConfig("config/bridge.json", c);
        if (rc != RC_SUCCESS)
            c = DefaultConfig();
        return c;
    }();
    static BridgeEngine engine(cfg);
    return engine;
}

} // namespace Bridge
