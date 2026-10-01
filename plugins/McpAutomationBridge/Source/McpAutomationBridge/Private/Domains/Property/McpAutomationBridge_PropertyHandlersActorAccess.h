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

// A material expression (or any object inside a material or material function) was written: tells the
// material or function it lives in that it changed, as compile_material does, so the parameter lists every
// instance reads are rebuilt and the material recompiled now. False when the object is in neither.
bool RefreshMaterialHostAfterEdit(UObject* Edited);
}
