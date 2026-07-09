#pragma once

#include "Domains/Render/McpAutomationBridge_RenderSupportSettings.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#if WITH_EDITOR
#include "Editor.h"
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
    if (!Volume)
    {
        Subsystem->SendAutomationError(Socket, RequestId, TEXT("PostProcessVolume not found."), TEXT("ACTOR_NOT_FOUND"));
    }
    return Volume;
}
#endif
}
