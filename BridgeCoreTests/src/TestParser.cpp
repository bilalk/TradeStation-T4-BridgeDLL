#include "TestFramework.h"
#include "../../BridgeCore/include/Parser.h"
#include "../../BridgeCore/include/Types.h"

void TestParser() {
    printf("\n-- TestParser --\n");

    // Valid PLACE via ParsePayload
    {
        Bridge::OrderRequest req;
        std::string payload =
            "command=PLACE|account=ACC1|instrument=ES|action=BUY|"
            "quantity=1|orderType=MARKET|limitPrice=0|stopPrice=0|timeInForce=DAY";
        int rc = Bridge::ParsePayload(payload, req);
        CHECK_EQ(rc, Bridge::RC_SUCCESS);
        CHECK_EQ((int)req.command,    (int)Bridge::Command::PLACE);
        CHECK_EQ((int)req.action,     (int)Bridge::Action::BUY);
        CHECK_EQ((int)req.orderType,  (int)Bridge::OrderType::MARKET);
        CHECK_EQ((int)req.timeInForce,(int)Bridge::TimeInForce::DAY);
        CHECK_EQ(req.quantity, 1);
        CHECK_TRUE(req.account == "ACC1");
        CHECK_TRUE(req.instrument == "ES");
    }

    // CANCEL payload (no action/quantity/orderType needed)
    {
        Bridge::OrderRequest req;
        std::string payload =
            "command=CANCEL|account=ACC1|instrument=NQ|action=BUY|"
            "quantity=0|orderType=MARKET|limitPrice=0|stopPrice=0|timeInForce=DAY";
        int rc = Bridge::ParsePayload(payload, req);
        CHECK_EQ(rc, Bridge::RC_SUCCESS);
        CHECK_EQ((int)req.command, (int)Bridge::Command::CANCEL);
    }

    // Bad command
    {
        Bridge::OrderRequest req;
        int rc = Bridge::ParsePayload("command=BADCMD", req);
        CHECK_EQ(rc, Bridge::RC_INVALID_CMD);
    }

    // Missing required field (PLACE without account)
    {
        Bridge::OrderRequest req;
        std::string payload =
            "command=PLACE|instrument=ES|action=BUY|"
            "quantity=1|orderType=MARKET|limitPrice=0|stopPrice=0|timeInForce=DAY";
        int rc = Bridge::ParsePayload(payload, req);
        CHECK_EQ(rc, Bridge::RC_INVALID_PARAM);
    }

    // LIMIT order with limitPrice
    {
        Bridge::OrderRequest req;
        std::string payload =
            "command=PLACE|account=ACC1|instrument=ES|action=BUY|"
            "quantity=5|orderType=LIMIT|limitPrice=4200.25|stopPrice=0|timeInForce=GTC";
        int rc = Bridge::ParsePayload(payload, req);
        CHECK_EQ(rc, Bridge::RC_SUCCESS);
        CHECK_EQ((int)req.orderType,   (int)Bridge::OrderType::LIMIT);
        CHECK_EQ((int)req.timeInForce, (int)Bridge::TimeInForce::GTC);
        CHECK_EQ(req.quantity, 5);
    }

    // BuildRequest (narrow) - valid
    {
        Bridge::OrderRequest req;
        int rc = Bridge::BuildRequest("PLACE","ACC1","ES","BUY",2,"MARKET",0.0,0.0,"DAY",req);
        CHECK_EQ(rc, Bridge::RC_SUCCESS);
        CHECK_EQ((int)req.command, (int)Bridge::Command::PLACE);
        CHECK_EQ(req.quantity, 2);
    }

    // BuildRequest (narrow) - invalid command
    {
        Bridge::OrderRequest req;
        int rc = Bridge::BuildRequest("BADCMD","ACC1","ES","BUY",2,"MARKET",0.0,0.0,"DAY",req);
        CHECK_EQ(rc, Bridge::RC_INVALID_CMD);
    }

    // BuildRequest (narrow) - null pointers
    {
        Bridge::OrderRequest req;
        int rc = Bridge::BuildRequest(nullptr,"ACC1","ES","BUY",2,"MARKET",0.0,0.0,"DAY",req);
        CHECK_EQ(rc, Bridge::RC_INVALID_CMD);
    }
}
