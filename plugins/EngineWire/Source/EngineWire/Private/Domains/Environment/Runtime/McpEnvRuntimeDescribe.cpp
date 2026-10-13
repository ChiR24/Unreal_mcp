#include "Domains/Environment/McpEnvShared.h"

namespace McpEnvironmentHandlers {

UWorld *McpGetRuntimeInspectionWorld()
{
    // A running PIE or standalone game world when there is one, else the editor world.
    if (GEngine)
    {
        for (const FWorldContext &Context : GEngine->GetWorldContexts())
        {
            if ((Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game) && Context.World())
            {
                return Context.World();
            }
        }
    }
    return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
}
FString McpGetWorldTypeName(UWorld *World)
{
    return World ? LexToString(World->WorldType) : TEXT("None");
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
// The values of the named properties (names or dotted paths) found on Object, or null when it has none of them;
// each name found is added to Found.
static TSharedPtr<FJsonObject> McpReadRuntimeNamedProperties(UObject *Object, const TArray<FString> &PropertyNames,
                                                             TSet<FString> *Found)
{
    TSharedPtr<FJsonObject> Values;
    for (const FString &PropertyName : PropertyNames)
    {
        if (PropertyName.IsEmpty())
        {
            continue;
        }
        McpHandlerUtils::FPropertyResolveResult PropResult = McpHandlerUtils::ResolveProperty(Object, PropertyName);
        if (!PropResult.IsValid())
        {
            continue;
        }
        if (TSharedPtr<FJsonValue> Value = McpPropertyReflection::ExportPropertyToJsonValue(PropResult.Container, PropResult.Property))
        {
            if (!Values)
            {
                Values = McpHandlerUtils::CreateResultObject();
            }
            Values->SetField(PropertyName, Value);
            if (Found)
            {
                Found->Add(PropertyName);
            }
        }
    }
    return Values;
}
TSharedPtr<FJsonObject> McpDescribeRuntimeComponent(UActorComponent *Component, const TArray<FString> &PropertyNames,
                                                    TSet<FString> *FoundProperties)
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
        Obj->SetObjectField(TEXT("transform"), McpHandlerUtils::TransformToJson(SceneComp->GetComponentTransform()));
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

    // A component carrying none of the names gets no properties field (it used to get an empty one each).
    if (TSharedPtr<FJsonObject> Values = McpReadRuntimeNamedProperties(Component, PropertyNames, FoundProperties))
    {
        Obj->SetObjectField(TEXT("properties"), Values);
    }

    return Obj;
}
TSharedPtr<FJsonObject> McpDescribeRuntimeActor(AActor *Actor, const TArray<FString> &ComponentNames, const TArray<FString> &PropertyNames,
                                                TSet<FString> *FoundProperties)
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
    Obj->SetObjectField(TEXT("transform"), McpHandlerUtils::TransformToJson(Actor->GetActorTransform()));
    McpAddActorTags(Obj, Actor);

    // propertyNames were read on the components only, so an actor's own variables (a Blueprint's bIsOpen) answered
    // with nothing. They are read on the actor too; the names found go in FoundProperties for the report.
    if (TSharedPtr<FJsonObject> Values = McpReadRuntimeNamedProperties(Actor, PropertyNames, FoundProperties))
    {
        Obj->SetObjectField(TEXT("properties"), Values);
    }

    TArray<TSharedPtr<FJsonValue>> ComponentsArray;
    TInlineComponentArray<UActorComponent *> Components;
    Actor->GetComponents(Components);
    // A bare report (no componentNames) used to describe EVERY component with
    // its full detail and per-component transform, which overflowed the gateway
    // result budget and turned a legal parameterless call into RESULT_TOO_LARGE.
    // Listing components is still useful; the per-component transform is what
    // made it heavy and is one inspect_object away, so it is kept only when the
    // caller named the component.
    constexpr int32 MaxReportedComponents = 12;
    const bool bNamedComponents = ComponentNames.Num() > 0;
    int32 OmittedComponents = 0;
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
            if (ComponentsArray.Num() >= MaxReportedComponents)
            {
                ++OmittedComponents;
                continue;
            }
            TSharedPtr<FJsonObject> ComponentObj = McpDescribeRuntimeComponent(Component, PropertyNames, FoundProperties);
            if (!bNamedComponents)
            {
                ComponentObj->RemoveField(TEXT("transform"));
            }
            ComponentsArray.Add(MakeShared<FJsonValueObject>(ComponentObj));
        }
    }
    Obj->SetArrayField(TEXT("components"), ComponentsArray);
    Obj->SetNumberField(TEXT("componentCount"), ComponentsArray.Num());
    if (OmittedComponents > 0)
    {
        Obj->SetNumberField(TEXT("componentsOmitted"), OmittedComponents);
    }
    return Obj;
}

} // namespace McpEnvironmentHandlers
