#pragma once

#include "CoreMinimal.h"
#include "Runtime/Launch/Resources/Version.h"

#ifndef MCP_HAS_PCG
#define MCP_HAS_PCG 0
#endif

#if MCP_HAS_PCG && (ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7))
class FJsonObject;
class UPCGBasePointData;

namespace McpPCGHandlers
{
// Validates the closed native point-field vocabulary without accessing point data.
bool ValidatePCGPointFields(const TArray<FString>& Fields, FString& OutError);

// Exports only explicitly selected native point fields; failure clears the output row.
bool ReadPCGPointRow(const UPCGBasePointData* Data, int32 Index, const TArray<FString>& Fields,
    TSharedPtr<FJsonObject>& OutRow, FString& OutError);
}
#endif
