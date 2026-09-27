#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Spline/McpAutomationBridge_SplineHandlersPrivate.h"

#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Components/SplineMeshComponent.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"

bool HandleSetSplineMeshMaterial(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    FString ComponentName = GetJsonStringField(Payload, TEXT("componentName"));
    FString MaterialPath = GetJsonStringField(Payload, TEXT("materialPath"));
    int32 MaterialIndex = GetJsonIntField(Payload, TEXT("materialIndex"), 0);

    if (ActorName.IsEmpty() || MaterialPath.IsEmpty())
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("actorName and materialPath are required"), nullptr, TEXT("MISSING_PARAM"));
        return true;
    }

    const FString SafeMaterialPath = RequireSplineProjectPath(Self, RequestId, Socket, TEXT("materialPath"), MaterialPath);
    if (SafeMaterialPath.IsEmpty())
    {
        return true;
    }

    AActor* Actor = nullptr;
    USplineMeshComponent* TargetComp = ResolveSplineMeshTarget(Self, RequestId, Socket, ActorName, ComponentName, Actor);
    if (!TargetComp)
    {
        return true;
    }

    UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *SafeMaterialPath);
    if (!Material)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Material not found: %s"), *SafeMaterialPath), nullptr, TEXT("MATERIAL_NOT_FOUND"));
        return true;
    }

    TargetComp->SetMaterial(MaterialIndex, Material);
    Actor->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("materialPath"), SafeMaterialPath);
    Result->SetNumberField(TEXT("materialIndex"), MaterialIndex);
    McpHandlerUtils::AddVerification(Result, Actor);
    AddComponentVerification(Result, TargetComp);

    Self->SendAutomationResponse(Socket, RequestId, true,
        TEXT("Spline mesh material set"), Result);
    return true;
}
