#pragma once

#include "CoreMinimal.h"
#include "Runtime/Launch/Resources/Version.h"

#ifndef MCP_HAS_PCG
#define MCP_HAS_PCG 0
#endif

#if MCP_HAS_PCG && (ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7))
class FProperty;
class FJsonObject;

namespace McpPCGHandlers
{
struct FPCGReadBudget;
bool SetPCGPropSchemaString(TSharedPtr<FJsonObject>& Object, const TCHAR* Field, const FString& Value, FString& OutError);
FString GetPCGPropSchemaTypeName(FProperty* Property);
bool BuildPCGPropSchemaType(FProperty* Property, const FString& TypeName, int32 Depth, FPCGReadBudget& Budget,
    TSharedPtr<FJsonObject>& OutDetails, FString& OutError);
}
#endif
