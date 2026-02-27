#include "MockAdapter.h"
#include "Types.h"
#include <string>
#include <sstream>

namespace Bridge {

int MockAdapter::Execute(const OrderRequest& req) {
    std::lock_guard<std::mutex> lk(m_mutex);
    switch (req.command) {
        case Command::PLACE:            return doPlace(req);
        case Command::CANCEL:           return doCancel(req);
        case Command::CANCELALLORDERS:  return doCancelAll(req);
        case Command::CHANGE:           return doChange(req);
        case Command::CLOSEPOSITION:    return doClosePosition(req);
        case Command::CLOSESTRATEGY:    // alias
        case Command::FLATTENEVERYTHING:return doFlattenEverything(req);
        case Command::REVERSEPOSITION:  return doReversePosition(req);
        default:                        return RC_INVALID_CMD;
    }
}

int MockAdapter::doPlace(const OrderRequest& req) {
    MockOrder o{};
    std::ostringstream oss;
    oss << "MOCK-" << m_nextId++;
    o.orderId    = oss.str();
    o.account    = req.account;
    o.instrument = req.instrument;
    o.action     = req.action;
    o.quantity   = req.quantity;
    o.orderType  = req.orderType;
    o.limitPrice = req.limitPrice;
    o.stopPrice  = req.stopPrice;
    o.timeInForce= req.timeInForce;
    o.working    = true;
    m_orders.push_back(std::move(o));
    return RC_SUCCESS;
}

int MockAdapter::doCancel(const OrderRequest& req) {
    // Cancel all working orders for account+instrument
    for (auto& o : m_orders) {
        if (o.account == req.account && o.instrument == req.instrument && o.working)
            o.working = false;
    }
    return RC_SUCCESS;
}

int MockAdapter::doCancelAll(const OrderRequest& req) {
    // Cancel all working orders regardless of instrument
    for (auto& o : m_orders) {
        if (o.account == req.account && o.working)
            o.working = false;
    }
    return RC_SUCCESS;
}

int MockAdapter::doChange(const OrderRequest& req) {
    // Cancel then place
    doCancel(req);
    return doPlace(req);
}

int MockAdapter::doClosePosition(const OrderRequest& req) {
    doCancel(req);
    return RC_SUCCESS;
}

int MockAdapter::doFlattenEverything(const OrderRequest& req) {
    (void)req;
    for (auto& o : m_orders)
        o.working = false;
    return RC_SUCCESS;
}

int MockAdapter::doReversePosition(const OrderRequest& req) {
    doCancel(req);
    // Synthetic reverse: place opposing order (flip action)
    OrderRequest rev = req;
    rev.action  = (req.action == Action::BUY) ? Action::SELL : Action::BUY;
    rev.command = Command::PLACE;
    return doPlace(rev);
}

} // namespace Bridge
