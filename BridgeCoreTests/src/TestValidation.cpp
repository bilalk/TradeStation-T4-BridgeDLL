#include "TestFramework.h"
#include "../../BridgeCore/include/Validation.h"
#include "../../BridgeCore/include/Types.h"

void TestValidation() {
    printf("\n-- TestValidation --\n");

    // ParseCommand
    CHECK_EQ((int)Bridge::ParseCommand("PLACE"),            (int)Bridge::Command::PLACE);
    CHECK_EQ((int)Bridge::ParseCommand("place"),            (int)Bridge::Command::PLACE);
    CHECK_EQ((int)Bridge::ParseCommand("CANCEL"),           (int)Bridge::Command::CANCEL);
    CHECK_EQ((int)Bridge::ParseCommand("CANCELALLORDERS"),  (int)Bridge::Command::CANCELALLORDERS);
    CHECK_EQ((int)Bridge::ParseCommand("CHANGE"),           (int)Bridge::Command::CHANGE);
    CHECK_EQ((int)Bridge::ParseCommand("CLOSEPOSITION"),    (int)Bridge::Command::CLOSEPOSITION);
    CHECK_EQ((int)Bridge::ParseCommand("CLOSESTRATEGY"),    (int)Bridge::Command::CLOSESTRATEGY);
    CHECK_EQ((int)Bridge::ParseCommand("FLATTENEVERYTHING"),(int)Bridge::Command::FLATTENEVERYTHING);
    CHECK_EQ((int)Bridge::ParseCommand("REVERSEPOSITION"),  (int)Bridge::Command::REVERSEPOSITION);
    CHECK_EQ((int)Bridge::ParseCommand("BADCMD"),           (int)Bridge::Command::UNKNOWN);
    CHECK_EQ((int)Bridge::ParseCommand(""),                 (int)Bridge::Command::UNKNOWN);

    // ParseAction
    CHECK_EQ((int)Bridge::ParseAction("BUY"),  (int)Bridge::Action::BUY);
    CHECK_EQ((int)Bridge::ParseAction("buy"),  (int)Bridge::Action::BUY);
    CHECK_EQ((int)Bridge::ParseAction("SELL"), (int)Bridge::Action::SELL);
    CHECK_EQ((int)Bridge::ParseAction("???"),  (int)Bridge::Action::UNKNOWN);

    // ParseOrderType
    CHECK_EQ((int)Bridge::ParseOrderType("MARKET"),     (int)Bridge::OrderType::MARKET);
    CHECK_EQ((int)Bridge::ParseOrderType("LIMIT"),      (int)Bridge::OrderType::LIMIT);
    CHECK_EQ((int)Bridge::ParseOrderType("STOPMARKET"), (int)Bridge::OrderType::STOPMARKET);
    CHECK_EQ((int)Bridge::ParseOrderType("STOPLIMIT"),  (int)Bridge::OrderType::STOPLIMIT);
    CHECK_EQ((int)Bridge::ParseOrderType("limit"),      (int)Bridge::OrderType::LIMIT);
    CHECK_EQ((int)Bridge::ParseOrderType("BAD"),        (int)Bridge::OrderType::UNKNOWN);

    // ParseTimeInForce
    CHECK_EQ((int)Bridge::ParseTimeInForce("DAY"), (int)Bridge::TimeInForce::DAY);
    CHECK_EQ((int)Bridge::ParseTimeInForce("GTC"), (int)Bridge::TimeInForce::GTC);
    CHECK_EQ((int)Bridge::ParseTimeInForce("day"), (int)Bridge::TimeInForce::DAY);
    CHECK_EQ((int)Bridge::ParseTimeInForce("???"), (int)Bridge::TimeInForce::UNKNOWN);

    // ValidateRequest - valid PLACE
    {
        Bridge::OrderRequest req;
        req.command     = Bridge::Command::PLACE;
        req.account     = "ACC1";
        req.instrument  = "ES";
        req.action      = Bridge::Action::BUY;
        req.quantity    = 1;
        req.orderType   = Bridge::OrderType::MARKET;
        req.timeInForce = Bridge::TimeInForce::DAY;
        CHECK_EQ(Bridge::ValidateRequest(req), Bridge::RC_SUCCESS);
    }

    // ValidateRequest - unknown command
    {
        Bridge::OrderRequest req;
        CHECK_EQ(Bridge::ValidateRequest(req), Bridge::RC_INVALID_CMD);
    }

    // ValidateRequest - PLACE missing account
    {
        Bridge::OrderRequest req;
        req.command     = Bridge::Command::PLACE;
        req.instrument  = "ES";
        req.action      = Bridge::Action::BUY;
        req.quantity    = 1;
        req.orderType   = Bridge::OrderType::MARKET;
        req.timeInForce = Bridge::TimeInForce::DAY;
        CHECK_EQ(Bridge::ValidateRequest(req), Bridge::RC_INVALID_PARAM);
    }

    // ValidateRequest - PLACE zero quantity
    {
        Bridge::OrderRequest req;
        req.command     = Bridge::Command::PLACE;
        req.account     = "ACC1";
        req.instrument  = "ES";
        req.action      = Bridge::Action::BUY;
        req.quantity    = 0;
        req.orderType   = Bridge::OrderType::MARKET;
        req.timeInForce = Bridge::TimeInForce::DAY;
        CHECK_EQ(Bridge::ValidateRequest(req), Bridge::RC_INVALID_PARAM);
    }

    // ValidateRequest - LIMIT order missing limitPrice
    {
        Bridge::OrderRequest req;
        req.command     = Bridge::Command::PLACE;
        req.account     = "ACC1";
        req.instrument  = "ES";
        req.action      = Bridge::Action::BUY;
        req.quantity    = 1;
        req.orderType   = Bridge::OrderType::LIMIT;
        req.limitPrice  = 0.0; // missing
        req.timeInForce = Bridge::TimeInForce::DAY;
        CHECK_EQ(Bridge::ValidateRequest(req), Bridge::RC_INVALID_PARAM);
    }

    // ValidateRequest - valid CANCEL
    {
        Bridge::OrderRequest req;
        req.command    = Bridge::Command::CANCEL;
        req.account    = "ACC1";
        req.instrument = "ES";
        CHECK_EQ(Bridge::ValidateRequest(req), Bridge::RC_SUCCESS);
    }

    // ValidateRequest - FLATTENEVERYTHING (no account/instrument required)
    {
        Bridge::OrderRequest req;
        req.command = Bridge::Command::FLATTENEVERYTHING;
        CHECK_EQ(Bridge::ValidateRequest(req), Bridge::RC_SUCCESS);
    }
}
