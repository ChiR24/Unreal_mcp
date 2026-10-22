#pragma once

#include "CoreMinimal.h"
#include "Runtime/Launch/Resources/Version.h"

#ifndef MCP_HAS_PCG
#define MCP_HAS_PCG 0
#endif

#if MCP_HAS_PCG && (ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7))
class UStruct;
class FJsonObject;

namespace McpPCGHandlers
{
// Describe reflected edit metadata only; this does not resolve values or promise write support.
bool DescribePCGPropertySchema(const UStruct* Scope, int32 Offset, int32 Limit,
    TSharedPtr<FJsonObject>& OutSchema, FString& OutError);
}
#endif
