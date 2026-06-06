#pragma once

#include "CoreMinimal.h"

class AActor;
class FMcpBridgeWebSocket;
class UMcpAutomationBridgeSubsystem;
class UWorld;

namespace VolumeHelpers
{
#if WITH_EDITOR
UWorld* GetEditorWorld();
AActor* FindVolumeByName(UWorld* World, const FString& VolumeName);
AActor* FindActorByPathOrName(UWorld* World, const FString& ActorPath);
bool ResolveEditorWorld(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket, UWorld*& OutWorld);
#endif
}
