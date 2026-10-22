#pragma once

#include "Domains/PCG/Data/McpPCGDataSnapshot.h"

#if MCP_HAS_PCG_EXECUTION
namespace McpPCGHandlers
{
struct FPCGDataProjection
{
    FString Domain = TEXT("Elements");
    TArray<FString> Fields, Attributes;
    int32 Offset = 0, Limit = 64;
};
// Captures an explicit bounded range, not an unknown-size full dataset. No cross-request native ranges survive.
bool BuildPCGDataRows(const UPCGData* Data, const FPCGDataProjection& Projection,
    TArray<TSharedPtr<FJsonValue>>& OutRows, int64& OutSourceTotal, FString& OutError);
}
#endif
