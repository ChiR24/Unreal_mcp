#include "McpAutomationBridge_EnvironmentHandlersShared.h"

#if WITH_EDITOR
namespace McpEnvironmentHandlers {

AActor *McpFindOrSpawnEnvironmentActor(const TSharedPtr<FJsonObject> &Payload, UClass *ActorClass, const FString &DefaultActorName)
{
    const FString ActorName = McpGetFirstStringField(Payload, {TEXT("targetActor"), TEXT("actorName"), TEXT("waterBodyName"), TEXT("name")});
    const FVector Location = McpGetVectorField(Payload, TEXT("location"), FVector::ZeroVector);
    const FRotator Rotation = McpGetRotatorField(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    return McpFindOrSpawnActor(ActorClass, ActorName.IsEmpty() ? DefaultActorName : ActorName, Location, Rotation);
}
void McpApplyEnvironmentSettings(UObject *Target, const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp)
{
    TArray<FString> Applied;
    TArray<FString> Failed;
    const int32 AppliedCount = McpApplyPayloadSettings(Target, Payload, Applied, Failed);
    Resp->SetNumberField(TEXT("configuredPropertyCount"), AppliedCount);
    McpAddStringArrayField(Resp, TEXT("configuredProperties"), Applied);
    McpAddStringArrayField(Resp, TEXT("configurationErrors"), Failed);
}
AActor *McpFindActorFromEnvironmentPayload(const TSharedPtr<FJsonObject> &Payload)
{
    const FString ActorName = McpGetFirstStringField(Payload, {TEXT("targetActor"), TEXT("actorName"), TEXT("waterBodyName"), TEXT("name"), TEXT("actorPath")});
    if (ActorName.IsEmpty())
    {
        return nullptr;
    }
    if (AActor *ActorByPath = FindObject<AActor>(nullptr, *ActorName))
    {
        return ActorByPath;
    }
    return McpFindActorByNameOrClass(nullptr, ActorName);
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
#endif
