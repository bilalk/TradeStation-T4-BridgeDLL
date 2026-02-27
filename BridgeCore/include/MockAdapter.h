#pragma once
#include "IBrokerAdapter.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>

namespace Bridge {

struct MockOrder {
    std::string orderId;
    std::string account;
    std::string instrument;
    Action      action;
    int         quantity;
    OrderType   orderType;
    double      limitPrice;
    double      stopPrice;
    TimeInForce timeInForce;
    bool        working; // true = open/working, false = cancelled/filled
};

class MockAdapter : public IBrokerAdapter {
public:
    MockAdapter() = default;

    bool IsConnected() const noexcept override { return true; }
    int  Execute(const OrderRequest& req) override;

    // Test helpers
    const std::vector<MockOrder>& GetOrders() const noexcept { return m_orders; }
    void Clear() noexcept { std::lock_guard<std::mutex> lk(m_mutex); m_orders.clear(); m_nextId = 1; }

private:
    std::vector<MockOrder> m_orders;
    std::mutex             m_mutex;
    int                    m_nextId = 1;

    int doPlace(const OrderRequest& req);
    int doCancel(const OrderRequest& req);
    int doCancelAll(const OrderRequest& req);
    int doChange(const OrderRequest& req);
    int doClosePosition(const OrderRequest& req);
    int doFlattenEverything(const OrderRequest& req);
    int doReversePosition(const OrderRequest& req);
};

} // namespace Bridge
