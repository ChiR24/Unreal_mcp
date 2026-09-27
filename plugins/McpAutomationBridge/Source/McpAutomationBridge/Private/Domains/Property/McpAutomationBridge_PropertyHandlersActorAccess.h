#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class AActor;
class FJsonValue;
class FMcpBridgeWebSocket;
class UMcpAutomationBridgeSubsystem;

namespace McpPropertyActorAccess
{
// ActorLocation / ActorRotation / ActorScale / ActorScale3D: the actor transform, which a Blueprint CDO does not have.
bool IsActorTransformProperty(const FString& PropertyName);

bool TryHandleSetActorProperty(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& PropertyName,
    const TSharedPtr<FJsonObject>& Payload,
    const TSharedPtr<FJsonValue>& ValueField,
    AActor* Actor,
    bool bIsClassDefaultObject,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);

void RefreshK2NodeTitleCacheIfNeeded(UObject* RootObject);
}
