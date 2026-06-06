#include "McpAutomationBridge_VolumeWorldResolution.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Engine/TriggerBase.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Volume.h"
#endif
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"

namespace VolumeHelpers
{
#if WITH_EDITOR
UWorld* GetEditorWorld()
{
    return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
}

AActor* FindVolumeByName(UWorld* World, const FString& VolumeName)
{
    if (!World || VolumeName.IsEmpty())
    {
        return nullptr;
    }
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (Actor && Actor->GetActorLabel().Equals(VolumeName, ESearchCase::IgnoreCase) &&
            (Actor->IsA<AVolume>() || Actor->IsA<ATriggerBase>()))
        {
            return Actor;
        }
    }
    return nullptr;
}

AActor* FindActorByPathOrName(UWorld* World, const FString& ActorPath)
{
    if (!World || ActorPath.IsEmpty())
    {
        return nullptr;
    }
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!Actor)
        {
            continue;
        }
        const FString ActorPathName = Actor->GetPathName();
        if (Actor->GetActorLabel().Equals(ActorPath, ESearchCase::IgnoreCase) ||
            Actor->GetName().Equals(ActorPath, ESearchCase::IgnoreCase) ||
            ActorPathName.Equals(ActorPath, ESearchCase::IgnoreCase) ||
            ActorPathName.EndsWith(ActorPath, ESearchCase::IgnoreCase))
        {
            return Actor;
        }
    }
    return nullptr;
}

bool ResolveEditorWorld(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket, UWorld*& OutWorld)
{
    OutWorld = GetEditorWorld();
    if (OutWorld)
    {
        return true;
    }
    Subsystem->SendAutomationResponse(Socket, RequestId, false, TEXT("Editor world not available"), nullptr);
    return false;
}
#endif
}
