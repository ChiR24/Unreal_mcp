#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

#if MCP_HAS_PCG
class UPCGSettings;

namespace McpPCGHandlers
{
// Handles only the two native static-mesh-spawner type fields, before dependent paths.
bool ApplyPCGSpawnerTypes(UPCGSettings* Settings, const TSharedPtr<FJsonObject>& Fields,
    TSet<FString>& OutHandledFields, FString& OutError);
}
#endif
