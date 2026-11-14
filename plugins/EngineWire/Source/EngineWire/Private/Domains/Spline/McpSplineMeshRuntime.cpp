#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Spline/McpSplinePrivate.h"

#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/McpScopedEditorTransaction.h"

#include "Components/SplineMeshComponent.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"

bool HandleSetSplineMeshMaterial(
    UEngineWireSubsystem* Self,
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
        Self->SendAutomationError(Socket, RequestId,
            TEXT("actorName and materialPath are required"), TEXT("MISSING_PARAMETER"));
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
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Material not found: %s"), *SafeMaterialPath), TEXT("MATERIAL_NOT_FOUND"));
        return true;
    }

    // SetMaterial grows the override list up to the index it is given, so an index past the component's slots is
    // refused: 2147483647 overflowed into the array's invalid-size fatal, and a few hundred million took gigabytes.
    if (MaterialIndex < 0 || MaterialIndex >= TargetComp->GetNumMaterials())
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("materialIndex %d is not a material slot of %s, which has %d: a spline mesh takes its slots from its mesh, so set "
                "the mesh first. Nothing was changed."), MaterialIndex, *TargetComp->GetName(), TargetComp->GetNumMaterials()),
            TEXT("MATERIAL_SLOT_NOT_FOUND"));
        return true;
    }

    FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Set Spline Mesh Material")), EMcpMutationDurability::EditorStateOnly,
                                            {TargetComp});
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
