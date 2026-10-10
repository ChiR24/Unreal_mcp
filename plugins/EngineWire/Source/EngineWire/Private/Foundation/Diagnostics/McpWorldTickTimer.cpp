#include "Foundation/Diagnostics/McpWorldTickTimer.h"

#include "Engine/World.h"
#include "HAL/PlatformTime.h"

namespace McpWorldTickTimer
{
namespace
{
struct FMcpWorldTickTimes
{
    double StartSeconds = 0.0;
    double AverageMs = -1.0;
};

FDelegateHandle GMcpWorldTickStartHandle;
FDelegateHandle GMcpWorldPostActorTickHandle;

TMap<TWeakObjectPtr<const UWorld>, FMcpWorldTickTimes>& McpWorldTickTable()
{
    static TMap<TWeakObjectPtr<const UWorld>, FMcpWorldTickTimes> Table;
    return Table;
}
}

void Start()
{
    if (GMcpWorldTickStartHandle.IsValid())
    {
        return;
    }
    GMcpWorldTickStartHandle = FWorldDelegates::OnWorldTickStart.AddLambda([](UWorld* World, ELevelTick, float)
    {
        TMap<TWeakObjectPtr<const UWorld>, FMcpWorldTickTimes>& Table = McpWorldTickTable();
        // Every PIE session is a new world: forget the ones that are gone before the table grows.
        if (Table.Num() > 16)
        {
            for (auto It = Table.CreateIterator(); It; ++It)
            {
                if (!It.Key().IsValid())
                {
                    It.RemoveCurrent();
                }
            }
        }
        if (World)
        {
            Table.FindOrAdd(World).StartSeconds = FPlatformTime::Seconds();
        }
    });
    GMcpWorldPostActorTickHandle = FWorldDelegates::OnWorldPostActorTick.AddLambda([](UWorld* World, ELevelTick, float)
    {
        FMcpWorldTickTimes* Times = World ? McpWorldTickTable().Find(World) : nullptr;
        if (!Times || Times->StartSeconds <= 0.0)
        {
            return;
        }
        const double Ms = (FPlatformTime::Seconds() - Times->StartSeconds) * 1000.0;
        Times->AverageMs = Times->AverageMs < 0.0 ? Ms : Times->AverageMs + (Ms - Times->AverageMs) * 0.1;
    });
}

void Stop()
{
    FWorldDelegates::OnWorldTickStart.Remove(GMcpWorldTickStartHandle);
    FWorldDelegates::OnWorldPostActorTick.Remove(GMcpWorldPostActorTickHandle);
    GMcpWorldTickStartHandle.Reset();
    GMcpWorldPostActorTickHandle.Reset();
    McpWorldTickTable().Reset();
}

double AverageMs(const UWorld* World)
{
    const FMcpWorldTickTimes* Times = World ? McpWorldTickTable().Find(World) : nullptr;
    return Times ? Times->AverageMs : -1.0;
}
}
