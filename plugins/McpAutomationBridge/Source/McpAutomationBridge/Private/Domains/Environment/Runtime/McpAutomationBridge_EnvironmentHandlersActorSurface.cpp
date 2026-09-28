#include "Domains/Environment/McpAutomationBridge_EnvironmentHandlersShared.h"

namespace McpEnvironmentHandlers {

AActor *McpFindOrSpawnEnvironmentActor(const TSharedPtr<FJsonObject> &Payload, UClass *ActorClass, const FString &DefaultActorName,
                                        bool bFirstOfClassWhenUnnamed)
{
    const FString ActorName = McpGetFirstStringField(Payload, {TEXT("targetActor"), TEXT("actorName"), TEXT("waterBodyName"), TEXT("name")});
    if (AActor *Existing = ActorName.IsEmpty() && bFirstOfClassWhenUnnamed ? McpFindActorByNameOrClass(ActorClass, FString()) : nullptr)
    {
        return Existing;
    }
    const FVector Location = ExtractVectorField(Payload, TEXT("location"), FVector::ZeroVector);
    const FRotator Rotation = ExtractRotatorField(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    return McpFindOrSpawnActor(ActorClass, ActorName.IsEmpty() ? DefaultActorName : ActorName, Location, Rotation);
}
int32 McpApplyEnvironmentSettings(UObject *Target, const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp)
{
    TArray<FString> Applied;
    TArray<FString> Failed;
    const int32 AppliedCount = McpApplyPayloadSettings(Target, Payload, Applied, Failed);
    Resp->SetNumberField(TEXT("configuredPropertyCount"), AppliedCount);
    McpAddStringArrayField(Resp, TEXT("configuredProperties"), Applied);
    McpAddStringArrayField(Resp, TEXT("configurationErrors"), Failed);
    return AppliedCount;
}
AActor *McpFindActorFromEnvironmentPayload(const TSharedPtr<FJsonObject> &Payload)
{
    const FString ActorName = McpGetFirstStringField(Payload, {TEXT("targetActor"), TEXT("actorName"), TEXT("waterBodyName"), TEXT("name"), TEXT("actorPath")});
    // Label, name or object path in the editor world.
    return FindActorByNameInWorldForMcp(McpHandlerUtils::GetEditorWorld(), ActorName, true);
}
AActor *McpFindWaterBodyActor(const TSharedPtr<FJsonObject> &Payload)
{
    const FString ActorName = McpGetFirstStringField(Payload, {TEXT("waterBodyName"), TEXT("targetActor"), TEXT("actorName"), TEXT("name")});
    const TArray<FString> ClassPaths = {
        TEXT("/Script/Water.WaterBodyOcean"), TEXT("/Script/Water.WaterBodyLake"),
        TEXT("/Script/Water.WaterBodyRiver"), TEXT("/Script/Water.WaterBodyCustom")
    };
    for (const FString &ClassPath : ClassPaths)
    {
        if (UClass *WaterClass = LoadClass<AActor>(nullptr, *ClassPath))
        {
            if (AActor *Actor = McpFindActorByNameOrClass(WaterClass, ActorName))
            {
                return Actor;
            }
        }
    }
    return ActorName.IsEmpty() ? nullptr : McpFindActorFromEnvironmentPayload(Payload);
}
int32 McpSetMaterialOnActor(AActor *Actor, const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp)
{
    FString MaterialPath;
    if (!Actor || !Payload->TryGetStringField(TEXT("materialPath"), MaterialPath) || MaterialPath.IsEmpty())
    {
        return 0;
    }
    UMaterialInterface *Material = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
    if (!Material)
    {
        Resp->SetStringField(TEXT("materialError"), FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
        return 0;
    }

    int32 MaterialIndex = 0;
    Payload->TryGetNumberField(TEXT("materialIndex"), MaterialIndex);
    int32 AppliedCount = 0;
    TInlineComponentArray<UPrimitiveComponent *> Components;
    Actor->GetComponents(Components);
    for (UPrimitiveComponent *Component : Components)
    {
        if (Component)
        {
            Component->Modify();
            Component->SetMaterial(MaterialIndex, Material);
            Component->MarkRenderStateDirty();
            ++AppliedCount;
        }
    }
    if (AppliedCount > 0)
    {
        Actor->MarkPackageDirty();
        Resp->SetStringField(TEXT("materialPath"), MaterialPath);
        Resp->SetNumberField(TEXT("materialComponentCount"), AppliedCount);
    }
    return AppliedCount;
}
int32 McpSetCollisionOnActor(AActor *Actor, const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp)
{
    bool bCollisionEnabled = true;
    if (!Actor || !Payload->TryGetBoolField(TEXT("collisionEnabled"), bCollisionEnabled))
    {
        return 0;
    }
    int32 AppliedCount = 0;
    TInlineComponentArray<UPrimitiveComponent *> Components;
    Actor->GetComponents(Components);
    for (UPrimitiveComponent *Component : Components)
    {
        if (Component)
        {
            Component->Modify();
            Component->SetCollisionEnabled(bCollisionEnabled ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
            Component->MarkRenderStateDirty();
            ++AppliedCount;
        }
    }
    Resp->SetBoolField(TEXT("collisionEnabled"), bCollisionEnabled);
    Resp->SetNumberField(TEXT("collisionComponentCount"), AppliedCount);
    return AppliedCount;
}

} // namespace McpEnvironmentHandlers
