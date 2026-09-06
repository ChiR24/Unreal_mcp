#pragma once

#include "Domains/Render/McpAutomationBridge_RenderSupportSettings.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/Render/McpPostProcessVolumeResolution.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/SceneCaptureCube.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UnrealType.h"
#endif

namespace McpRenderHandlers
{
#if WITH_EDITOR
inline UWorld* GetRenderWorld()
{
    if (!GEditor)
    {
        return nullptr;
    }
    return GEditor->PlayWorld
        ? GEditor->PlayWorld.Get()
        : GEditor->GetEditorWorldContext().World();
}

inline AActor* FindRenderActor(const FString& Reference)
{
    if (Reference.IsEmpty())
    {
        return nullptr;
    }
    if (AActor* ByPath = FindObject<AActor>(nullptr, *Reference))
    {
        return ByPath;
    }
    UWorld* World = GetRenderWorld();
    if (!World)
    {
        return nullptr;
    }
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (Actor &&
            (Actor->GetName().Equals(Reference, ESearchCase::IgnoreCase) ||
             Actor->GetActorLabel().Equals(Reference, ESearchCase::IgnoreCase) ||
             Actor->GetPathName().Equals(Reference, ESearchCase::IgnoreCase)))
        {
            return Actor;
        }
    }
    return nullptr;
}

inline TSharedPtr<FJsonObject> MakeRenderResult(const FString& SubAction)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("action"), TEXT("manage_render"));
    Result->SetStringField(TEXT("subAction"), SubAction);
    Result->SetBoolField(TEXT("success"), true);
    Result->SetBoolField(TEXT("applied"), true);
    return Result;
}

inline APostProcessVolume* RequirePostProcessVolume(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString Reference;
    ReadActorReference(Payload, Reference);
    APostProcessVolume* Volume = Cast<APostProcessVolume>(FindRenderActor(Reference));
    if (!Volume && Reference.IsEmpty())
    {
        // No explicit reference: use the same deterministic resolver as the
        // lens/exposure handlers (single unbound volume, persistent-level
        // preference, spawn one when the level has none). Failing on an empty
        // name split the post-process family into two behaviours for the same
        // level: half AMBIGUOUS, half "PostProcessVolume not found".
        FString ResolveError;
        FString ResolveErrorCode;
        Volume = McpResolvePostProcessVolume(GetRenderWorld(), Payload, true, ResolveError, ResolveErrorCode);
        if (!Volume)
        {
            Subsystem->SendAutomationError(
                Socket, RequestId,
                ResolveError.IsEmpty() ? FString(TEXT("PostProcessVolume not found.")) : ResolveError,
                ResolveErrorCode.IsEmpty() ? FString(TEXT("ACTOR_NOT_FOUND")) : ResolveErrorCode);
        }
        return Volume;
    }
    if (!Volume)
    {
        Subsystem->SendAutomationError(
            Socket, RequestId,
            FString::Printf(TEXT("PostProcessVolume not found: %s"), *Reference),
            TEXT("ACTOR_NOT_FOUND"));
    }
    return Volume;
}

// Scene-capture actions declare an optional actorName; when it is omitted and
// the level holds exactly one scene capture actor, that actor is the target.
// More than one is reported as AMBIGUOUS with the candidate labels so the
// caller can pick; none is reported as ACTOR_NOT_FOUND.
inline AActor* FindSoleSceneCaptureActor(FString& OutError, FString& OutErrorCode)
{
    OutError.Reset();
    OutErrorCode.Reset();
    UWorld* World = GetRenderWorld();
    if (!World)
    {
        OutError = TEXT("No valid world available.");
        OutErrorCode = TEXT("NO_WORLD");
        return nullptr;
    }
    TArray<AActor*> Captures;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (Actor && (Actor->IsA<ASceneCapture2D>() || Actor->IsA<ASceneCaptureCube>()))
        {
            Captures.Add(Actor);
        }
    }
    if (Captures.Num() == 1)
    {
        return Captures[0];
    }
    if (Captures.Num() > 1)
    {
        TArray<FString> Labels;
        for (AActor* Actor : Captures)
        {
            Labels.Add(FString::Printf(TEXT("%s (%s)"), *Actor->GetActorLabel(), *Actor->GetName()));
        }
        OutError = FString::Printf(
            TEXT("Multiple scene capture actors found (%d); pass actorName to pick one. Candidates: %s"),
            Captures.Num(), *FString::Join(Labels, TEXT(", ")));
        OutErrorCode = TEXT("AMBIGUOUS");
        return nullptr;
    }
    OutError = TEXT("Scene capture actor not found: no SceneCapture2D/SceneCaptureCube in the level (create one with create_scene_capture_2d or pass actorName).");
    OutErrorCode = TEXT("ACTOR_NOT_FOUND");
    return nullptr;
}
#endif
}
