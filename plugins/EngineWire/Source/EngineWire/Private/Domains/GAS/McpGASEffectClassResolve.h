#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Foundation/BridgeHelpers/Reflection/McpClassResolve.h"

namespace McpGASHandlers
{
// A class derived from RequiredBase from any shape callers pass, through Foundation's ResolveClassByName:
// "/Script/Module.Class", a Blueprint package path ("/Game/X/GEC", the path create_execution_calculation hands back),
// an object path ("/Game/X/GEC.GEC"), a generated-class path ("/Game/X/GEC.GEC_C") or a bare class name.
static inline UClass* ResolveGASClass(const FString& InPath, UClass* RequiredBase)
{
    UClass* Class = RequiredBase ? ResolveClassByName(InPath.TrimStartAndEnd()) : nullptr;
    return Class && Class->IsChildOf(RequiredBase) ? Class : nullptr;
}
}
