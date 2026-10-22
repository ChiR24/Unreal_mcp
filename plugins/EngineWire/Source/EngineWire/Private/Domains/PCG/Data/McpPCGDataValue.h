#pragma once

#include "CoreMinimal.h"
#include "Runtime/Launch/Resources/Version.h"

#ifndef MCP_HAS_PCG
#define MCP_HAS_PCG 0
#endif

#if MCP_HAS_PCG && (ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7))
#include "Metadata/PCGMetadataCommon.h"

class FPCGMetadataAttributeBase;
class FJsonValue;

namespace McpPCGDataValue
{
// Supports exactly the 15 typed EPCGMetadataTypes through SoftClassPath on UE 5.7+; newer generic types are refused.
// Reads default/inherited values without mutation or object loading; failure clears OutValue and explains the refusal.
bool ExportPCGDataValue(const FPCGMetadataAttributeBase* Attribute, PCGMetadataEntryKey EntryKey,
    TSharedPtr<FJsonValue>& OutValue, FString& OutError);
}
#endif
