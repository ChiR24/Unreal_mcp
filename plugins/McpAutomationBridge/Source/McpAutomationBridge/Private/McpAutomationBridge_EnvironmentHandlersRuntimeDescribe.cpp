#include "McpAutomationBridge_EnvironmentHandlersShared.h"

#if WITH_EDITOR
namespace McpEnvironmentHandlers {

UWorld *McpGetRuntimeInspectionWorld()
{
    if (!GEditor)
    {
        return nullptr;
    }

    if (GEditor->PlayWorld)
    {
        return GEditor->PlayWorld.Get();
    }

    if (GEngine)
    {
        for (const FWorldContext &Context : GEngine->GetWorldContexts())
        {
            if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
            {
                if (UWorld *World = Context.World())
                {
                    return World;
                }
            }
        }
    }

    return GEditor->GetEditorWorldContext().World();
}
FString McpGetWorldTypeName(UWorld *World)
{
    if (!World)
    {
        return TEXT("None");
    }

    switch (World->WorldType)
    {
    case EWorldType::PIE:
        return TEXT("PIE");
    case EWorldType::Game:
        return TEXT("Game");
    case EWorldType::Editor:
        return TEXT("Editor");
    case EWorldType::EditorPreview:
        return TEXT("EditorPreview");
    case EWorldType::GamePreview:
        return TEXT("GamePreview");
    case EWorldType::GameRPC:
        return TEXT("GameRPC");
    case EWorldType::Inactive:
        return TEXT("Inactive");
    default:
        return TEXT("Unknown");
    }
}
void McpAddActorTags(TSharedPtr<FJsonObject> Obj, const AActor *Actor)
{
    TArray<TSharedPtr<FJsonValue>> TagsArray;
    if (Actor)
    {
        for (const FName &Tag : Actor->Tags)
        {
            TagsArray.Add(MakeShared<FJsonValueString>(Tag.ToString()));
        }
    }
    Obj->SetArrayField(TEXT("tags"), TagsArray);
}
TSharedPtr<FJsonObject> McpDescribeRuntimeComponent(UActorComponent *Component, const TArray<FString> &PropertyNames)
{
    TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
    if (!Component)
    {
        return Obj;
    }

    Obj->SetStringField(TEXT("name"), Component->GetName());
    Obj->SetStringField(TEXT("path"), Component->GetPathName());
    Obj->SetStringField(TEXT("class"), Component->GetClass() ? Component->GetClass()->GetName() : TEXT(""));
    Obj->SetStringField(TEXT("classPath"), Component->GetClass() ? Component->GetClass()->GetPathName() : TEXT(""));
    Obj->SetBoolField(TEXT("isActive"), Component->IsActive());

    if (USceneComponent *SceneComp = Cast<USceneComponent>(Component))
    {
        Obj->SetBoolField(TEXT("isSceneComponent"), true);
        Obj->SetBoolField(TEXT("isVisible"), SceneComp->IsVisible());
        Obj->SetObjectField(TEXT("transform"), McpMakeTransformObject(SceneComp->GetComponentTransform()));
        Obj->SetStringField(TEXT("attachParent"), SceneComp->GetAttachParent() ? SceneComp->GetAttachParent()->GetName() : TEXT(""));
    }

    if (UCameraComponent *CameraComp = Cast<UCameraComponent>(Component))
    {
        Obj->SetBoolField(TEXT("isCamera"), true);
        Obj->SetNumberField(TEXT("fieldOfView"), CameraComp->FieldOfView);
        Obj->SetBoolField(TEXT("isActive"), CameraComp->IsActive());
    }

    if (USpringArmComponent *SpringArm = Cast<USpringArmComponent>(Component))
    {
        Obj->SetBoolField(TEXT("isSpringArm"), true);
        Obj->SetNumberField(TEXT("targetArmLength"), SpringArm->TargetArmLength);
        Obj->SetBoolField(TEXT("usePawnControlRotation"), SpringArm->bUsePawnControlRotation);
    }

    if (PropertyNames.Num() > 0)
    {
        TSharedPtr<FJsonObject> PropertiesObj = McpHandlerUtils::CreateResultObject();
        for (const FString &PropertyName : PropertyNames)
        {
            if (PropertyName.IsEmpty())
            {
                continue;
            }
            McpHandlerUtils::FPropertyResolveResult PropResult = McpHandlerUtils::ResolveProperty(Component, PropertyName);
            if (PropResult.IsValid())
            {
                if (TSharedPtr<FJsonValue> Value = ExportPropertyToJsonValue(PropResult.Container, PropResult.Property))
                {
                    PropertiesObj->SetField(PropertyName, Value);
                }
            }
        }
        Obj->SetObjectField(TEXT("properties"), PropertiesObj);
    }

    return Obj;
}
TSharedPtr<FJsonObject> McpDescribeRuntimeActor(AActor *Actor, const TArray<FString> &ComponentNames, const TArray<FString> &PropertyNames)
{
    TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
    if (!Actor)
    {
        return Obj;
    }

    Obj->SetStringField(TEXT("name"), Actor->GetName());
    Obj->SetStringField(TEXT("label"), Actor->GetActorLabel());
    Obj->SetStringField(TEXT("path"), Actor->GetPathName());
    Obj->SetStringField(TEXT("class"), Actor->GetClass() ? Actor->GetClass()->GetName() : TEXT(""));
    Obj->SetStringField(TEXT("classPath"), Actor->GetClass() ? Actor->GetClass()->GetPathName() : TEXT(""));
    Obj->SetObjectField(TEXT("transform"), McpMakeTransformObject(Actor->GetActorTransform()));
    McpAddActorTags(Obj, Actor);

    TArray<TSharedPtr<FJsonValue>> ComponentsArray;
    TInlineComponentArray<UActorComponent *> Components;
    Actor->GetComponents(Components);
    for (UActorComponent *Component : Components)
    {
        if (!Component)
        {
            continue;
        }

        const bool bRequestedByName = ComponentNames.Num() == 0 || ComponentNames.ContainsByPredicate([Component](const FString &RequestedName) {
            return Component->GetName().Equals(RequestedName, ESearchCase::IgnoreCase);
        });
        const bool bAlwaysReportCameraState = Component->IsA<UCameraComponent>() || Component->IsA<USpringArmComponent>();
        if (bRequestedByName || bAlwaysReportCameraState)
        {
            ComponentsArray.Add(MakeShared<FJsonValueObject>(McpDescribeRuntimeComponent(Component, PropertyNames)));
        }
    }
    Obj->SetArrayField(TEXT("components"), ComponentsArray);
    Obj->SetNumberField(TEXT("componentCount"), ComponentsArray.Num());
    return Obj;
}

} // namespace McpEnvironmentHandlers
#endif
