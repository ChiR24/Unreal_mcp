#include "Domains/Volume/McpVolumeWorldResolve.h"

#include "Editor.h"
#include "Engine/TriggerBase.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Volume.h"
#include "EngineWireSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"

namespace VolumeHelpers
{
AActor* FindVolumeByName(UWorld* World, const FString& VolumeName)
{
    AActor* Actor = FindActorByNameInWorldForMcp(World, VolumeName, true);
    return Actor && (Actor->IsA<AVolume>() || Actor->IsA<ATriggerBase>()) ? Actor : nullptr;
}

bool ResolveEditorWorld(UEngineWireSubsystem* Subsystem, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket, UWorld*& OutWorld)
{
    OutWorld = GetEditorWorld();
    if (OutWorld)
    {
        return true;
    }
    Subsystem->SendAutomationResponse(Socket, RequestId, false, TEXT("Editor world not available"), nullptr);
    return false;
}
}
