#include "DotNetAdapterStub.h"
#include "Types.h"

namespace Bridge {

int DotNetAdapterStub::Execute(const OrderRequest&) {
    return RC_NOT_CONNECTED;
}

} // namespace Bridge
