#include "Domains/Interaction/McpAutomationBridge_InteractionHandlersPrivate.h"

namespace McpInteractionHandlers
{
bool HandleSwitchAction(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString SwitchType = GetJsonStringField(Payload, TEXT("switchType"), TEXT("button"));
    if (SubAction == TEXT("create_switch_actor"))
    {
        UBlueprint* SwitchBP = CreateInteractableBlueprint(Subsystem, RequestId, RequestingSocket, Payload, TEXT("/Game/Interactables"), TEXT("switch"), {
            {USceneComponent::StaticClass(), TEXT("Root"), nullptr, 0.0f},
            {UStaticMeshComponent::StaticClass(), TEXT("SwitchMesh"), nullptr, 0.0f},
            {USphereComponent::StaticClass(), TEXT("InteractionTrigger"), nullptr, 100.0f}});
        if (!SwitchBP)
        {
            return true;
        }
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("switchPath"), SwitchBP->GetPathName());
        Result->SetStringField(TEXT("blueprintPath"), SwitchBP->GetPathName());
        Result->SetStringField(TEXT("switchType"), SwitchType);
        SendInteractableResult(Subsystem, RequestId, RequestingSocket, SwitchBP, Result,
                               {TEXT("created switch blueprint")}, TEXT("Switch actor created"));
        return true;
    }

    if (SubAction != TEXT("configure_switch_properties"))
    {
        return false;
    }
    UBlueprint* Blueprint = LoadInteractableBlueprint(Subsystem, RequestId, RequestingSocket, Payload,
                                                      TEXT("switchPath"), TEXT("switch"), {TEXT("SwitchMesh")});
    if (!Blueprint)
    {
        return true;
    }
    const bool CanToggle = GetJsonBoolField(Payload, TEXT("canToggle"), true);
    const double ResetTime = GetJsonNumberField(Payload, TEXT("resetTime"), 0.0);
    using EType = EInteractionVarType;
    const int32 NotApplied = ApplyInteractionVars(Blueprint, {
        {TEXT("SwitchType"), EType::Name, MakeShared<FJsonValueString>(SwitchType)},
        {TEXT("bCanToggle"), EType::Bool, MakeShared<FJsonValueBoolean>(CanToggle)},
        {TEXT("bIsActivated"), EType::Bool, nullptr},
        {TEXT("ResetTime"), EType::Float, MakeShared<FJsonValueNumber>(ResetTime)}});
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("switchType"), SwitchType);
    Result->SetBoolField(TEXT("canToggle"), CanToggle);
    Result->SetNumberField(TEXT("resetTime"), ResetTime);
    Result->SetBoolField(TEXT("configured"), true);
    Result->SetBoolField(TEXT("propertiesApplied"), NotApplied == 0);
    Result->SetStringField(TEXT("switchPath"), GetJsonStringField(Payload, TEXT("switchPath")));
    SendInteractableResult(Subsystem, RequestId, RequestingSocket, Blueprint, Result,
                           {TEXT("configured switch properties")}, TEXT("Switch properties configured"));
    return true;
}
}
