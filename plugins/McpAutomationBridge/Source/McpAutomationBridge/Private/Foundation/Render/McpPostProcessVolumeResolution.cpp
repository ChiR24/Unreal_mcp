#include "Foundation/Render/McpPostProcessVolumeResolution.h"
#include "Domains/Render/McpAutomationBridge_RenderSupportSettings.h"

#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#if WITH_EDITOR
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace McpRenderHandlers
{
APostProcessVolume* McpResolvePostProcessVolume(
    UWorld* World,
    const TSharedPtr<FJsonObject>& Payload,
    bool bAllowSpawn,
    FString& OutError,
    FString& OutErrorCode)
{
    OutError.Reset();
    OutErrorCode.Reset();

    if (!World)
    {
        OutError = TEXT("No valid world available for PostProcessVolume resolution.");
        OutErrorCode = TEXT("NO_WORLD");
        return nullptr;
    }

    // Optional explicit actor reference (actorName / targetActor / actorPath).
    // The post-process, exposure and screen records declare `actorName` so a
    // caller can always pick one volume explicitly.
    FString Reference;
    if (Payload.IsValid())
    {
        Reference = GetJsonStringField(Payload, TEXT("actorName"));
        if (Reference.IsEmpty())
        {
            Reference = GetJsonStringField(Payload, TEXT("targetActor"));
        }
        if (Reference.IsEmpty())
        {
            Reference = GetJsonStringField(Payload, TEXT("actorPath"));
        }
    }
    if (!Reference.IsEmpty())
    {
        for (TActorIterator<APostProcessVolume> It(World); It; ++It)
        {
            APostProcessVolume* Candidate = *It;
            if (Candidate &&
                (Candidate->GetName().Equals(Reference, ESearchCase::IgnoreCase) ||
                 Candidate->GetActorLabel().Equals(Reference, ESearchCase::IgnoreCase) ||
                 Candidate->GetPathName().Equals(Reference, ESearchCase::IgnoreCase)))
            {
                return Candidate;
            }
        }
    }

    // Deterministic class-based resolution over unbound volumes.
    TArray<APostProcessVolume*> Unbound;
    for (TActorIterator<APostProcessVolume> It(World); It; ++It)
    {
        APostProcessVolume* Candidate = *It;
        if (Candidate && Candidate->bUnbound)
        {
            Unbound.Add(Candidate);
        }
    }

    if (Unbound.Num() == 1)
    {
        return Unbound[0];
    }
    if (Unbound.Num() > 1)
    {
        // A loaded streaming sub-level routinely carries its own unbound volume
        // next to the persistent level's one. Prefer the persistent level: that
        // is the volume the level author sees as "the" post-process volume, and
        // without this preference every post-process action failed AMBIGUOUS in
        // any level that streams a sub-level.
        TArray<APostProcessVolume*> Persistent;
        for (APostProcessVolume* Candidate : Unbound)
        {
            if (Candidate->GetLevel() == World->PersistentLevel)
            {
                Persistent.Add(Candidate);
            }
        }
        if (Persistent.Num() == 1)
        {
            return Persistent[0];
        }
        const TArray<APostProcessVolume*>& Ambiguous = Persistent.Num() > 1 ? Persistent : Unbound;
        TArray<FString> CandidateLabels;
        for (APostProcessVolume* Candidate : Ambiguous)
        {
            CandidateLabels.Add(FString::Printf(
                TEXT("%s (%s)"), *Candidate->GetActorLabel(), *Candidate->GetPathName()));
        }
        OutError = FString::Printf(
            TEXT("Multiple unbound PostProcessVolumes found (%d); pass actorName to pick one. Candidates: %s"),
            Ambiguous.Num(), *FString::Join(CandidateLabels, TEXT(", ")));
        OutErrorCode = TEXT("AMBIGUOUS");
        return nullptr;
    }

    // Zero unbound volumes.
    if (bAllowSpawn)
    {
        APostProcessVolume* PPV = Cast<APostProcessVolume>(
            SpawnActorInActiveWorld<AActor>(
                APostProcessVolume::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator));
        if (PPV)
        {
            PPV->bUnbound = true;
            return PPV;
        }
        OutError = TEXT("Failed to spawn PostProcessVolume.");
        OutErrorCode = TEXT("EXECUTION_ERROR");
        return nullptr;
    }

    OutError = TEXT("PostProcessVolume not found.");
    OutErrorCode = TEXT("ACTOR_NOT_FOUND");
    return nullptr;
}

// BB-021: bounded post-process volume settings summary so exposure/bloom/blend
// mutations can be independently verified through inspect/component reads.
TSharedPtr<FJsonObject> McpDescribePostProcessVolume(const APostProcessVolume& Volume)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("enabled"), Volume.bEnabled);
    Result->SetBoolField(TEXT("unbound"), Volume.bUnbound);
    Result->SetNumberField(TEXT("priority"), static_cast<double>(Volume.Priority));
    Result->SetNumberField(TEXT("blendRadius"), static_cast<double>(Volume.BlendRadius));
    Result->SetNumberField(TEXT("blendWeight"), static_cast<double>(Volume.BlendWeight));
    Result->SetNumberField(TEXT("exposureBias"), static_cast<double>(Volume.Settings.AutoExposureBias));
    Result->SetNumberField(TEXT("bloomIntensity"), static_cast<double>(Volume.Settings.BloomIntensity));
    Result->SetNumberField(
        TEXT("ambientOcclusionIntensity"), static_cast<double>(Volume.Settings.AmbientOcclusionIntensity));
    return Result;
}
}
#endif
