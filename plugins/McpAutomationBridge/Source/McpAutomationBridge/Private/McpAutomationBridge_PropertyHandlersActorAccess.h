#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class AActor;
class FJsonValue;
class FMcpBridgeWebSocket;
class UMcpAutomationBridgeSubsystem;

namespace McpPropertyActorAccess
{
void AddObjectVerification(TSharedPtr<FJsonObject>& Result, UObject* Object);

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
