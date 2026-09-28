#include "Domains/Environment/McpAutomationBridge_EnvironmentHandlersShared.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsTransforms.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Materials/MaterialInterface.h"

namespace McpEnvironmentHandlers {

namespace {
// get_material_details, get_mesh_details and get_texture_details answered any
// object with the generic inspect_object view, so asking for a material's details
// on a mesh "succeeded". Each now names the type it reads; empty = no check.
FString McpInspectWrongTypeReason(const FString &Action, const UObject *Object)
{
    if (Action.Equals(TEXT("get_material_details"), ESearchCase::IgnoreCase) && !Object->IsA<UMaterialInterface>())
    {
        return TEXT("a material");
    }
    if (Action.Equals(TEXT("get_mesh_details"), ESearchCase::IgnoreCase) &&
        !Object->IsA<UStaticMesh>() && !Object->IsA<USkeletalMesh>())
    {
        return TEXT("a static or skeletal mesh");
    }
    if (Action.Equals(TEXT("get_texture_details"), ESearchCase::IgnoreCase) && !Object->IsA<UTexture>())
    {
        return TEXT("a texture");
    }
    return FString();
}
} // namespace

bool HandleInspectObjectAction(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const FString &InitialObjectPath, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString ObjectPath = InitialObjectPath;
    FString ResolvedPath;
    UObject* TargetObject = McpHandlerUtils::ResolveObjectFromPath(ObjectPath, &ResolvedPath);

    if (!TargetObject)
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Object not found: %s"), *ObjectPath),
                            TEXT("OBJECT_NOT_FOUND"));
        return true;
    }

    // Update path for error messages
    if (!ResolvedPath.IsEmpty())
    {
        ObjectPath = ResolvedPath;
    }

    FString Action;
    Payload->TryGetStringField(TEXT("action"), Action);
    const FString Wanted = McpInspectWrongTypeReason(Action, TargetObject);
    if (!Wanted.IsEmpty())
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("%s is a %s, not %s; use inspect_object for any object"),
                            *ObjectPath, *TargetObject->GetClass()->GetName(), *Wanted),
            TEXT("TYPE_MISMATCH"));
        return true;
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();

    Resp->SetStringField(TEXT("objectPath"), TargetObject->GetPathName());
    Resp->SetStringField(TEXT("objectName"), TargetObject->GetName());
    Resp->SetStringField(TEXT("className"), TargetObject->GetClass()->GetName());
    Resp->SetStringField(TEXT("classPath"), TargetObject->GetClass()->GetPathName());
    Resp->SetStringField(TEXT("class"), TargetObject->GetClass()->GetName());
    Resp->SetBoolField(TEXT("isAsset"), TargetObject->IsAsset());

    if (AActor *Actor = Cast<AActor>(TargetObject))
    {
        Resp->SetStringField(TEXT("actorLabel"), Actor->GetActorLabel());
        Resp->SetBoolField(TEXT("isActor"), true);
        Resp->SetBoolField(TEXT("isHidden"), Actor->IsHidden());
        Resp->SetBoolField(TEXT("isSelected"), Actor->IsSelected());

        TSharedPtr<FJsonObject> TransformObj = McpHandlerUtils::CreateResultObject();
        const FTransform &Transform = Actor->GetActorTransform();

        TransformObj->SetObjectField(TEXT("location"), McpHandlerUtils::VectorToJson(Transform.GetLocation()));

        TSharedPtr<FJsonObject> RotationObj = McpHandlerUtils::CreateResultObject();
        FRotator Rotator = Transform.GetRotation().Rotator();
        RotationObj->SetNumberField(TEXT("pitch"), Rotator.Pitch);
        RotationObj->SetNumberField(TEXT("yaw"), Rotator.Yaw);
        RotationObj->SetNumberField(TEXT("roll"), Rotator.Roll);
        TransformObj->SetObjectField(TEXT("rotation"), RotationObj);

        TransformObj->SetObjectField(TEXT("scale"), McpHandlerUtils::VectorToJson(Transform.GetScale3D()));

        Resp->SetObjectField(TEXT("transform"), TransformObj);

        TArray<TSharedPtr<FJsonValue>> ComponentsArray;
        TInlineComponentArray<UActorComponent *> Components;
        Actor->GetComponents(Components);

        for (UActorComponent *Component : Components)
        {
            if (Component)
            {
                TSharedPtr<FJsonObject> CompObj = McpHandlerUtils::CreateResultObject();
                CompObj->SetStringField(TEXT("name"), Component->GetName());
                CompObj->SetStringField(TEXT("class"), Component->GetClass()->GetName());
                CompObj->SetBoolField(TEXT("isActive"), Component->IsActive());

                // Add specific info for common component types
                if (USceneComponent *SceneComp = Cast<USceneComponent>(Component))
                {
                    CompObj->SetBoolField(TEXT("isSceneComponent"), true);
                    CompObj->SetBoolField(TEXT("isVisible"), SceneComp->IsVisible());
                }

                if (UStaticMeshComponent *MeshComp = Cast<UStaticMeshComponent>(Component))
                {
                    CompObj->SetBoolField(TEXT("isStaticMesh"), true);
                    if (MeshComp->GetStaticMesh())
                    {
                        CompObj->SetStringField(TEXT("staticMesh"), MeshComp->GetStaticMesh()->GetName());
                    }
                }

                ComponentsArray.Add(MakeShared<FJsonValueObject>(CompObj));
            }
        }
        Resp->SetArrayField(TEXT("components"), ComponentsArray);
        Resp->SetNumberField(TEXT("componentCount"), ComponentsArray.Num());
    }
    else
    {
        Resp->SetBoolField(TEXT("isActor"), false);
    }

    // Tags: the placed actor's own tags (the CDO's tags were reported before,
    // which hid tags added to the instance); an empty array for non-actors.
    McpAddActorTags(Resp, Cast<AActor>(TargetObject));
    if (UActorComponent *Component = Cast<UActorComponent>(TargetObject))
    {
        McpDescribeComponent(Component, Resp);
    }
    // componentName: describe and read that component, not the actor. It was
    // declared but never read, so a light's Intensity (which lives on its light
    // component) came back under missingProperties.
    UObject *DumpTarget = TargetObject;
    FString ComponentName;
    AActor *Owner = Cast<AActor>(TargetObject);
    if (Owner && Payload->TryGetStringField(TEXT("componentName"), ComponentName) && !ComponentName.IsEmpty())
    {
        UActorComponent *Named = FindComponentByName(Owner, ComponentName);
        if (!Named)
        {
            TArray<FString> Names;
            for (UActorComponent *Each : TInlineComponentArray<UActorComponent *>(Owner))
            {
                Names.Add(Each ? Each->GetName() : FString());
            }
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Component '%s' not found on %s. Its components: %s."), *ComponentName,
                                *Owner->GetActorLabel(), *FString::Join(Names, TEXT(", "))),
                TEXT("COMPONENT_NOT_FOUND"));
            return true;
        }
        McpDescribeComponent(Named, Resp);
        DumpTarget = Named;
    }
    // detailed / propertyNames: UPROPERTY values as text, capped at 200 entries.
    const TArray<FString> PropertyNames = McpReadStringListField(Payload, TEXT("propertyNames"), TEXT("propertyName"));
    if (PropertyNames.Num() > 0 || GetJsonBoolField(Payload, TEXT("detailed"), false))
    {
        McpAppendPropertyDump(DumpTarget, PropertyNames, Resp);
    }
    // Material / mesh / texture / Blueprint specifics; a no-op for other objects.
    McpDescribeAssetDetails(TargetObject, Resp);

    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Object inspection completed"), Resp, FString());
    return true;
}

} // namespace McpEnvironmentHandlers
