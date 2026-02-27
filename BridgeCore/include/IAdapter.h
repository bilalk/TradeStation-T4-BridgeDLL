#pragma once
#include "OrderRequest.h"

// Adapter interface – all connectivity back-ends implement this.
struct IAdapter {
    virtual int  Execute(const OrderRequest& req) = 0;
    virtual bool IsConnected() const              = 0;
    virtual ~IAdapter()                           = default;
};
