#include "Domains/Interaction/McpAutomationBridge_InteractionHandlersPrivate.h"

namespace McpInteractionHandlers
{
bool HandleChestAction(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const bool Locked = GetJsonBoolField(Payload, TEXT("locked"), false);
    using EType = EInteractionVarType;
    if (SubAction == TEXT("create_chest_actor"))
    {
        UBlueprint* ChestBP = CreateInteractableBlueprint(Subsystem, RequestId, RequestingSocket, Payload, TEXT("/Game/Interactables"), TEXT("chest"), {
            {USceneComponent::StaticClass(), TEXT("Root"), nullptr, 0.0f},
            {UStaticMeshComponent::StaticClass(), TEXT("ChestBase"), nullptr, 0.0f},
            {USceneComponent::StaticClass(), TEXT("LidPivot"), nullptr, 0.0f},
            {UStaticMeshComponent::StaticClass(), TEXT("LidMesh"), TEXT("LidPivot"), 0.0f},
            {USphereComponent::StaticClass(), TEXT("InteractionTrigger"), nullptr, 150.0f}});
        if (!ChestBP)
        {
            return true;
        }
        // create_chest_actor used to echo `locked` and store it nowhere, so every chest came out unlocked.
        const int32 NotApplied = ApplyInteractionVars(ChestBP, {
            {TEXT("bIsLocked"), EType::Bool, MakeShared<FJsonValueBoolean>(Locked)}});
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("chestPath"), ChestBP->GetPathName());
        Result->SetStringField(TEXT("blueprintPath"), ChestBP->GetPathName());
        Result->SetBoolField(TEXT("locked"), Locked);
        Result->SetBoolField(TEXT("propertiesApplied"), NotApplied == 0);
        SendInteractableResult(Subsystem, RequestId, RequestingSocket, ChestBP, Result,
                               {TEXT("created chest blueprint"), TEXT("added chest components")}, TEXT("Chest actor created"));
        return true;
    }

    if (SubAction != TEXT("configure_chest_properties"))
    {
        return false;
    }
    UBlueprint* Blueprint = LoadInteractableBlueprint(Subsystem, RequestId, RequestingSocket, Payload,
                                                      TEXT("chestPath"), TEXT("chest"), {TEXT("ChestBase"), TEXT("LidMesh")});
    if (!Blueprint)
    {
        return true;
    }
    const double OpenAngle = GetJsonNumberField(Payload, TEXT("openAngle"), 90.0);
    const double OpenTime = GetJsonNumberField(Payload, TEXT("openTime"), 0.5);
    const FString LootTablePath = GetJsonStringField(Payload, TEXT("lootTablePath"));
    // Only the fields the caller passed are written; defaulting the rest used to reset them.
    const int32 NotApplied = ApplyInteractionVars(Blueprint, {
        {TEXT("bIsLocked"), EType::Bool, Payload->TryGetField(TEXT("locked"))},
        {TEXT("bIsOpen"), EType::Bool, nullptr},
        {TEXT("LidOpenAngle"), EType::Float, Payload->TryGetField(TEXT("openAngle"))},
        {TEXT("OpenTime"), EType::Float, Payload->TryGetField(TEXT("openTime"))},
        {TEXT("LootTable"), EType::SoftObject,
         LootTablePath.IsEmpty() ? TSharedPtr<FJsonValue>() : MakeShared<FJsonValueString>(LootTablePath)}});
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    if (Payload->HasField(TEXT("locked"))) { Result->SetBoolField(TEXT("locked"), Locked); }
    if (Payload->HasField(TEXT("openAngle"))) { Result->SetNumberField(TEXT("openAngle"), OpenAngle); }
    if (Payload->HasField(TEXT("openTime"))) { Result->SetNumberField(TEXT("openTime"), OpenTime); }
    if (!LootTablePath.IsEmpty())
    {
        Result->SetStringField(TEXT("lootTablePath"), LootTablePath);
    }
    Result->SetBoolField(TEXT("configured"), true);
    Result->SetBoolField(TEXT("propertiesApplied"), NotApplied == 0);
    Result->SetStringField(TEXT("chestPath"), GetJsonStringField(Payload, TEXT("chestPath")));
    SendInteractableResult(Subsystem, RequestId, RequestingSocket, Blueprint, Result,
                           {TEXT("configured chest properties")}, TEXT("Chest properties configured"));
    return true;
}
}
