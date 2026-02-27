#include "FixAdapterStub.h"
#include "Types.h"

namespace Bridge {

int FixAdapterStub::Execute(const OrderRequest&) {
    return RC_NOT_CONNECTED;
}

} // namespace Bridge
