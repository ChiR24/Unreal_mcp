#include "Domains/Environment/McpAutomationBridge_EnvironmentHandlersShared.h"

#if WITH_EDITOR
namespace McpEnvironmentHandlers {

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

        TSharedPtr<FJsonObject> LocationObj = McpHandlerUtils::CreateResultObject();
        LocationObj->SetNumberField(TEXT("x"), Transform.GetLocation().X);
        LocationObj->SetNumberField(TEXT("y"), Transform.GetLocation().Y);
        LocationObj->SetNumberField(TEXT("z"), Transform.GetLocation().Z);
        TransformObj->SetObjectField(TEXT("location"), LocationObj);

        TSharedPtr<FJsonObject> RotationObj = McpHandlerUtils::CreateResultObject();
        FRotator Rotator = Transform.GetRotation().Rotator();
        RotationObj->SetNumberField(TEXT("pitch"), Rotator.Pitch);
        RotationObj->SetNumberField(TEXT("yaw"), Rotator.Yaw);
        RotationObj->SetNumberField(TEXT("roll"), Rotator.Roll);
        TransformObj->SetObjectField(TEXT("rotation"), RotationObj);

        TSharedPtr<FJsonObject> ScaleObj = McpHandlerUtils::CreateResultObject();
        ScaleObj->SetNumberField(TEXT("x"), Transform.GetScale3D().X);
        ScaleObj->SetNumberField(TEXT("y"), Transform.GetScale3D().Y);
        ScaleObj->SetNumberField(TEXT("z"), Transform.GetScale3D().Z);
        TransformObj->SetObjectField(TEXT("scale"), ScaleObj);

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
        UActorComponent *Named = McpHandlerUtils::FindActorComponentByName(Owner, ComponentName);
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
    if (PropertyNames.Num() > 0 || McpHandlerUtils::GetOptionalBool(Payload, TEXT("detailed"), false))
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
#endif
