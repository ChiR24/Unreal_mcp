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
    const bool bCapturedErrorsTruncated,
    const TArray<FString>& CapturedWarnings,
    const int32 TotalCapturedWarningCount,
    const bool bCapturedWarningsTruncated)
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

    // A warning the engine logged while the request ran was captured and then
    // thrown away, so "Saved 0 packages" or a material that failed to find a
    // parameter answered as a clean success and nothing surfaced until someone
    // opened the Output Log. Warnings ride the same channel as errors now, one
    // rung quieter.
    TArray<TSharedPtr<FJsonValue>> WarningValues;
    if (Result.IsValid())
    {
        const TArray<TSharedPtr<FJsonValue>>* ExistingWarnings = nullptr;
        if (Result->TryGetArrayField(TEXT("warnings"), ExistingWarnings) && ExistingWarnings)
        {
            WarningValues = *ExistingWarnings;
        }
    }

    const int32 MaxCapturedInResponse = 3;
    if (CapturedWarnings.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> EngineWarningValues;
        const int32 WarningResponseCount =
            FMath::Min(CapturedWarnings.Num(), MaxCapturedInResponse);
        for (int32 WarningIndex = 0; WarningIndex < WarningResponseCount; ++WarningIndex)
        {
            EngineWarningValues.Add(MakeShared<FJsonValueString>(
                SanitizeEngineErrorForResponse(CapturedWarnings[WarningIndex])));
        }
        Enriched->SetBoolField(TEXT("engineWarningsObserved"), true);
        Enriched->SetNumberField(TEXT("engineWarningCount"), TotalCapturedWarningCount);
        Enriched->SetArrayField(TEXT("engineWarnings"), EngineWarningValues);
        if (bCapturedWarningsTruncated || CapturedWarnings.Num() > MaxCapturedInResponse)
        {
            Enriched->SetBoolField(TEXT("engineWarningsTruncated"), true);
        }
        for (const TSharedPtr<FJsonValue>& WarningValue : EngineWarningValues)
        {
            WarningValues.Add(WarningValue);
        }
    }

    if (CapturedErrors.Num() == 0)
    {
        if (WarningValues.Num() > 0)
        {
            Enriched->SetArrayField(TEXT("warnings"), WarningValues);
        }
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
    WarningValues.Add(MakeShared<FJsonValueString>(FString::Printf(
        TEXT("%d engine error(s) were logged while this request ran. The handler still reports success; ")
        TEXT("see engineErrors for the captured text."),
        TotalCapturedErrorCount)));
    Enriched->SetArrayField(TEXT("warnings"), WarningValues);
    return Enriched;
}
}
