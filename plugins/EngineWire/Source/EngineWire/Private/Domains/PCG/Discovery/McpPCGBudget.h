#pragma once

#include "Domains/PCG/McpPCGPrivate.h"

#if MCP_HAS_PCG
namespace McpPCGHandlers
{
// Shared across dependency graphs. Refuse before invoking unbounded native text/JSON export.
struct FPCGReadBudget
{
    static constexpr int32 MaxValues = 65536, MaxObjects = 8192, MaxNodes = 4096, MaxEdges = 16384;
    static constexpr int64 MaxCharacters = 1048576;
    static constexpr double MaxSeconds = 0.1;
    double Deadline = FPlatformTime::Seconds() + MaxSeconds;
    bool IsWithinTime() const { return FPlatformTime::Seconds() <= Deadline; }
    int32 Values = 0, Objects = 0, Nodes = 0, Edges = 0, VisitedObjects = 0;
    int64 Characters = 0;
    bool CheckGraph(UPCGGraph* Graph);
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)
    bool CheckParameters(const FInstancedPropertyBag* Bag);
#endif
    bool CheckObject(UObject* Object);
private:
    bool CheckValue(const FProperty* Property, void* Value, int32 Depth);
};
}
#endif
