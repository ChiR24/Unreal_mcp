#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Core/Subsystem/McpAutomationBridgeSubsystemResponseSanitization.h"

// Enriches an outgoing automation result with transport-level context that the handler itself
// cannot know, without ever overriding the handler's own verdict.
//
// Two things are attached:
//   * the world the request ran against (WORLD-01). An actor mutation reports success for the world
//     that was current when it ran; if a level load replaces that world, the receipt gives the caller
//     no way to notice. Naming the world makes the mismatch detectable instead of invisible.
//   * engine-log errors observed during the request. These are ATTACHED for the caller to judge; the
//     handler's success verdict deliberately stands, because downgrading it conflated transport success
//     with asset-level warnings and produced false negatives (a handler that completed its work was
//     reported as failed, triggering pointless retries and undo-then-reapply flows).
namespace McpAutomationBridgeSubsystemResponse
{
inline TSharedPtr<FJsonObject> McpBuildEnrichedResponseResult(
    const TSharedPtr<FJsonObject>& Result,
    const FString& WorldName,
    const bool bIsTransientWorld,
    const TArray<FString>& CapturedErrors,
    const int32 TotalCapturedErrorCount,
    const bool bCapturedErrorsTruncated)
{
    TSharedPtr<FJsonObject> Enriched = MakeShared<FJsonObject>();
    if (Result.IsValid())
    {
        for (const auto& Pair : Result->Values)
        {
            Enriched->SetField(Pair.Key, Pair.Value);
        }
    }

    if (!WorldName.IsEmpty())
    {
        Enriched->SetStringField(TEXT("worldName"), WorldName);
        if (bIsTransientWorld)
        {
            // A /Temp package is unsaved and is discarded if a level load completes, so an actor
            // reported under it may not be reachable by the time the caller acts on this receipt.
            Enriched->SetBoolField(TEXT("isTransientWorld"), true);
        }
    }

    if (CapturedErrors.Num() == 0)
    {
        return Enriched;
    }

    TArray<TSharedPtr<FJsonValue>> ErrorValues;
    const int32 MaxErrorsInResponse = 3;
    const int32 ErrorResponseCount = FMath::Min(CapturedErrors.Num(), MaxErrorsInResponse);
    for (int32 ErrorIndex = 0; ErrorIndex < ErrorResponseCount; ++ErrorIndex)
    {
        ErrorValues.Add(MakeShared<FJsonValueString>(
            SanitizeEngineErrorForResponse(CapturedErrors[ErrorIndex])));
    }
    Enriched->SetBoolField(TEXT("engineErrorsObserved"), true);
    Enriched->SetNumberField(TEXT("engineErrorCount"), TotalCapturedErrorCount);
    Enriched->SetArrayField(TEXT("engineErrors"), ErrorValues);
    if (bCapturedErrorsTruncated || CapturedErrors.Num() > MaxErrorsInResponse)
    {
        Enriched->SetBoolField(TEXT("engineErrorsTruncated"), true);
    }

    // The errors were only reachable under `details`, while `warnings` is the channel a caller watches
    // for "it succeeded, but read this" -- so a mutation that tripped 32 engine errors, including an
    // ensure, looked completely clean to anyone inspecting warnings. Mirror a bounded summary there.
    TArray<TSharedPtr<FJsonValue>> WarningValues;
    if (Result.IsValid())
    {
        const TArray<TSharedPtr<FJsonValue>>* ExistingWarnings = nullptr;
        if (Result->TryGetArrayField(TEXT("warnings"), ExistingWarnings) && ExistingWarnings)
        {
            WarningValues = *ExistingWarnings;
        }
    }
    WarningValues.Add(MakeShared<FJsonValueString>(FString::Printf(
        TEXT("%d engine error(s) were logged while this request ran. The handler still reports success; ")
        TEXT("see engineErrors for the captured text."),
        TotalCapturedErrorCount)));
    Enriched->SetArrayField(TEXT("warnings"), WarningValues);
    return Enriched;
}
}
