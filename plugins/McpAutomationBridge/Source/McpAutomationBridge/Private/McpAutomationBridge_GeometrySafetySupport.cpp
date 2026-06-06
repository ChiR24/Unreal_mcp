#include "McpAutomationBridge_GeometryHandlers.h"

#if WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT

namespace McpGeometryHandlers
{
bool IsMemoryPressureSafe()
{
#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
    FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();
    double UsagePercent = static_cast<double>(MemStats.UsedPhysical) /
                          static_cast<double>(MemStats.TotalPhysical);
    return UsagePercent < MEMORY_PRESSURE_CRITICAL;
#else
    return true;  // Assume safe on other platforms
#endif
}

double GetMemoryUsagePercent()
{
#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
    FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();
    return static_cast<double>(MemStats.UsedPhysical) /
           static_cast<double>(MemStats.TotalPhysical) * 100.0;
#else
    return 0.0;
#endif
}

int32 ClampSegments(int32 Value, int32 Default)
{
    return FMath::Clamp(Value <= 0 ? Default : Value, 1, MAX_SEGMENTS);
}

double ClampDimension(double Value, double Default)
{
    if (Value <= 0.0) Value = Default;
    return FMath::Clamp(Value, MIN_DIMENSION, MAX_DIMENSION);
}
} // namespace McpGeometryHandlers

#endif // WITH_EDITOR && MCP_HAS_FULL_GEOMETRY_SCRIPT
