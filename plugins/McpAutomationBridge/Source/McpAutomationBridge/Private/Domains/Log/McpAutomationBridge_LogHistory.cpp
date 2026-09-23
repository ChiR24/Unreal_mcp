#include "Domains/Log/McpAutomationBridge_LogHistory.h"

#include "Algo/Reverse.h"
#include "CoreGlobals.h"
#include "HAL/PlatformTime.h"
#include "Logging/LogVerbosity.h"
#include "Misc/ScopeLock.h"

FMcpLogHistory& FMcpLogHistory::Get()
{
    static FMcpLogHistory Instance;
    return Instance;
}

void FMcpLogHistory::Register()
{
    if (!bRegistered && GLog)
    {
        GLog->AddOutputDevice(this);
        bRegistered = true;
    }
}

void FMcpLogHistory::Unregister()
{
    if (bRegistered && GLog)
    {
        GLog->RemoveOutputDevice(this);
    }
    bRegistered = false;
}

void FMcpLogHistory::Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category)
{
    if (!V)
    {
        return;
    }
    FLine Line;
    Line.Seconds = FPlatformTime::Seconds() - GStartTime;
    Line.Category = Category;
    Line.Verbosity = static_cast<ELogVerbosity::Type>(Verbosity & ELogVerbosity::VerbosityMask);
    Line.Message = FString(V).Left(1024);

    FScopeLock Lock(&Mutex);
    if (Lines.Num() < Capacity)
    {
        Lines.Add(MoveTemp(Line));
        return;
    }
    Lines[Head] = MoveTemp(Line);
    Head = (Head + 1) % Capacity;
}

TArray<FString> FMcpLogHistory::Read(int32 MaxLines, const FString& Contains, const FString& Category,
                                     ELogVerbosity::Type MinVerbosity, int32& OutMatched) const
{
    TArray<FString> Out;
    OutMatched = 0;
    FScopeLock Lock(&Mutex);
    const int32 Count = Lines.Num();
    // Newest first so the cap keeps the most recent matches; Head is the oldest
    // slot once the ring is full and 0 before, so Head - 1 is always the newest.
    for (int32 Offset = 1; Offset <= Count; ++Offset)
    {
        const FLine& Line = Lines[(Head - Offset + Count) % Count];
        if (Line.Verbosity == ELogVerbosity::NoLogging || Line.Verbosity > MinVerbosity ||
            (!Category.IsEmpty() && !Line.Category.ToString().Equals(Category, ESearchCase::IgnoreCase)) ||
            // The text filter also matches the category, so "LiveCoding" finds
            // LogLiveCoding lines whose message spells it "Live Coding".
            (!Contains.IsEmpty() && !Line.Message.Contains(Contains, ESearchCase::IgnoreCase) &&
             !Line.Category.ToString().Contains(Contains, ESearchCase::IgnoreCase)))
        {
            continue;
        }
        ++OutMatched;
        if (Out.Num() < MaxLines)
        {
            Out.Add(FString::Printf(TEXT("[%.3f] %s: %s: %s"), Line.Seconds, *Line.Category.ToString(),
                                    ToString(Line.Verbosity), *Line.Message));
        }
    }
    Algo::Reverse(Out);
    return Out;
}
