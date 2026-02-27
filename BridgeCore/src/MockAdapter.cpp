#include "IAdapter.h"
#include "Logger.h"
#include "ResultCodes.h"
#include <string>

// ---------------------------------------------------------------------------
// MockAdapter – always succeeds; useful for testing without connectivity.
// ---------------------------------------------------------------------------
class MockAdapter : public IAdapter {
public:
    int Execute(const OrderRequest& req) override {
        std::string cmdStr;
        switch (req.cmd) {
            case OrderCmd::PLACE:  cmdStr = "PLACE";  break;
            case OrderCmd::CANCEL: cmdStr = "CANCEL"; break;
            case OrderCmd::MODIFY: cmdStr = "MODIFY"; break;
        }
        Logger::Log("[MockAdapter] Execute cmd=" + cmdStr
            + " sym=" + req.symbol
            + " acct=" + req.account
            + " qty=" + std::to_string(req.qty)
            + " px=" + std::to_string(req.price)
            + " side=" + req.side
            + " type=" + req.type);
        return RC_SUCCESS;
    }

    bool IsConnected() const override { return true; }
};

// Factory used by the DLL layer.
IAdapter* CreateMockAdapter() { return new MockAdapter(); }
