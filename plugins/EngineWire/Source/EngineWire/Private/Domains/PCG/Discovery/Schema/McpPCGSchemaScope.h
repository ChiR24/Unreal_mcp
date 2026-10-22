#pragma once

#include "Domains/PCG/McpPCGPrivate.h"

#if MCP_HAS_PCG && (ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7))
namespace McpPCGHandlers
{
// Borrowed pointers for one synchronous GT query only, never a retained handle or an authorization credential.
struct FPCGSchemaScope
{
    UPCGSettings* Settings = nullptr;
    UObject* Object = nullptr;
    const UStruct* Type = nullptr;
    const void* Container = nullptr;
    FString Path;
};

bool ResolvePCGSchemaScope(UPCGGraph* Graph, UPCGNode* Node, const FString& ScopePath,
    FPCGSchemaScope& OutScope, FString& OutError);
}
#endif
