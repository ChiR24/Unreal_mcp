// UBT requires X.cpp to include X.h first. The private umbrella below already
// pulls it in, but the check cannot see through the umbrella, so naming it here
// keeps the build output free of a spurious per-build error.
#include "MCP/Transport/McpNativeTransport.h"

#include "MCP/Transport/McpNativeTransportPrivate.h"
