#pragma once

#include "Domains/PCG/McpPCGPrivate.h"

#if MCP_HAS_PCG
namespace McpPCGHandlers
{
bool ValidatePCGConnection(const UPCGPin* Source, const UPCGPin* Target, FString& OutError, FString& OutCode);
TArray<TSharedPtr<FJsonValue>> ValidatePCGEdges(UPCGGraph* Graph);
}
#endif
