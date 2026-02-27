#include "TestFramework.h"
#include "../../BridgeCore/include/MockAdapter.h"
#include "../../BridgeCore/include/Types.h"

static Bridge::OrderRequest MakePlaceReq(
    const std::string& account,
    const std::string& instrument,
    Bridge::Action action,
    int qty,
    Bridge::OrderType ot = Bridge::OrderType::MARKET)
{
    Bridge::OrderRequest r;
    r.command     = Bridge::Command::PLACE;
    r.account     = account;
    r.instrument  = instrument;
    r.action      = action;
    r.quantity    = qty;
    r.orderType   = ot;
    r.timeInForce = Bridge::TimeInForce::DAY;
    return r;
}

void TestMockAdapter() {
    printf("\n-- TestMockAdapter --\n");

    Bridge::MockAdapter adapter;
    CHECK_TRUE(adapter.IsConnected());

    // Place order
    {
        adapter.Clear();
        auto req = MakePlaceReq("ACC1","ES",Bridge::Action::BUY,1);
        int rc = adapter.Execute(req);
        CHECK_EQ(rc, Bridge::RC_SUCCESS);
        CHECK_EQ((int)adapter.GetOrders().size(), 1);
        CHECK_TRUE(adapter.GetOrders()[0].working);
        CHECK_TRUE(adapter.GetOrders()[0].orderId == "MOCK-1");
    }

    // Place multiple
    {
        adapter.Clear();
        auto r1 = MakePlaceReq("ACC1","ES",Bridge::Action::BUY,1);
        auto r2 = MakePlaceReq("ACC1","NQ",Bridge::Action::SELL,2);
        adapter.Execute(r1);
        adapter.Execute(r2);
        CHECK_EQ((int)adapter.GetOrders().size(), 2);
    }

    // Cancel (account+instrument)
    {
        adapter.Clear();
        adapter.Execute(MakePlaceReq("ACC1","ES",Bridge::Action::BUY,1));
        adapter.Execute(MakePlaceReq("ACC1","NQ",Bridge::Action::BUY,1));

        Bridge::OrderRequest cancelReq;
        cancelReq.command    = Bridge::Command::CANCEL;
        cancelReq.account    = "ACC1";
        cancelReq.instrument = "ES";
        int rc = adapter.Execute(cancelReq);
        CHECK_EQ(rc, Bridge::RC_SUCCESS);

        const auto& orders = adapter.GetOrders();
        CHECK_FALSE(orders[0].working); // ES order cancelled
        CHECK_TRUE (orders[1].working); // NQ order still working
    }

    // CancelAllOrders
    {
        adapter.Clear();
        adapter.Execute(MakePlaceReq("ACC1","ES",Bridge::Action::BUY,1));
        adapter.Execute(MakePlaceReq("ACC1","NQ",Bridge::Action::BUY,1));

        Bridge::OrderRequest req;
        req.command  = Bridge::Command::CANCELALLORDERS;
        req.account  = "ACC1";
        int rc = adapter.Execute(req);
        CHECK_EQ(rc, Bridge::RC_SUCCESS);

        const auto& orders = adapter.GetOrders();
        CHECK_FALSE(orders[0].working);
        CHECK_FALSE(orders[1].working);
    }

    // Change (cancel + place)
    {
        adapter.Clear();
        auto r = MakePlaceReq("ACC1","ES",Bridge::Action::BUY,1);
        adapter.Execute(r);
        CHECK_EQ((int)adapter.GetOrders().size(), 1);
        CHECK_TRUE(adapter.GetOrders()[0].working);

        Bridge::OrderRequest changeReq = r;
        changeReq.command  = Bridge::Command::CHANGE;
        changeReq.quantity = 5;
        int rc = adapter.Execute(changeReq);
        CHECK_EQ(rc, Bridge::RC_SUCCESS);
        // Old cancelled, new placed
        CHECK_EQ((int)adapter.GetOrders().size(), 2);
        CHECK_FALSE(adapter.GetOrders()[0].working);
        CHECK_TRUE (adapter.GetOrders()[1].working);
        CHECK_EQ(adapter.GetOrders()[1].quantity, 5);
    }

    // FlattenEverything
    {
        adapter.Clear();
        adapter.Execute(MakePlaceReq("ACC1","ES",Bridge::Action::BUY,1));
        adapter.Execute(MakePlaceReq("ACC2","NQ",Bridge::Action::SELL,2));

        Bridge::OrderRequest req;
        req.command = Bridge::Command::FLATTENEVERYTHING;
        int rc = adapter.Execute(req);
        CHECK_EQ(rc, Bridge::RC_SUCCESS);

        const auto& orders = adapter.GetOrders();
        CHECK_FALSE(orders[0].working);
        CHECK_FALSE(orders[1].working);
    }

    // CloseStrategy == FlattenEverything
    {
        adapter.Clear();
        adapter.Execute(MakePlaceReq("ACC1","ES",Bridge::Action::BUY,1));

        Bridge::OrderRequest req;
        req.command    = Bridge::Command::CLOSESTRATEGY;
        req.account    = "ACC1";
        req.instrument = "ES";
        int rc = adapter.Execute(req);
        CHECK_EQ(rc, Bridge::RC_SUCCESS);
        CHECK_FALSE(adapter.GetOrders()[0].working);
    }

    // ReversePosition
    {
        adapter.Clear();
        adapter.Execute(MakePlaceReq("ACC1","ES",Bridge::Action::BUY,3));

        Bridge::OrderRequest req = MakePlaceReq("ACC1","ES",Bridge::Action::BUY,3);
        req.command = Bridge::Command::REVERSEPOSITION;
        int rc = adapter.Execute(req);
        CHECK_EQ(rc, Bridge::RC_SUCCESS);
        // Original cancelled, new SELL order placed
        CHECK_EQ((int)adapter.GetOrders().size(), 2);
        CHECK_FALSE(adapter.GetOrders()[0].working);
        CHECK_TRUE (adapter.GetOrders()[1].working);
        CHECK_EQ((int)adapter.GetOrders()[1].action, (int)Bridge::Action::SELL);
    }

    // Unknown command
    {
        Bridge::OrderRequest req;
        req.command = Bridge::Command::UNKNOWN;
        int rc = adapter.Execute(req);
        CHECK_EQ(rc, Bridge::RC_INVALID_CMD);
    }
}
