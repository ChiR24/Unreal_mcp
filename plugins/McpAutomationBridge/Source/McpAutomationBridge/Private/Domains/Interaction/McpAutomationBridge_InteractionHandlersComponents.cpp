#include "Domains/Interaction/McpAutomationBridge_InteractionHandlersPrivate.h"

namespace McpInteractionHandlers
{
bool HandleInteractionComponentAuthoringAction(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const double TraceDistance = GetJsonNumberField(Payload, TEXT("traceDistance"), 200.0);
    if (SubAction == TEXT("create_interaction_component"))
    {
        UBlueprint* Blueprint = LoadInteractableBlueprint(Subsystem, RequestId, RequestingSocket, Payload,
                                                          TEXT("blueprintPath"), TEXT("interactable"), {});
        if (!Blueprint)
        {
            return true;
        }
        if (!Blueprint->SimpleConstructionScript)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint has no SimpleConstructionScript"), TEXT("INVALID_BP"));
            return true;
        }
        const FString ComponentName = GetJsonStringField(Payload, TEXT("componentName"), TEXT("InteractionComponent"));
        USCS_Node* Node = Blueprint->SimpleConstructionScript->CreateNode(USphereComponent::StaticClass(), *ComponentName);
        if (!Node)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create interaction component"), TEXT("COMPONENT_CREATE_FAILED"));
            return true;
        }
        ConfigureInteractionShape(Node->ComponentTemplate, static_cast<float>(TraceDistance));
        Blueprint->SimpleConstructionScript->AddNode(Node);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetBoolField(TEXT("componentAdded"), true);
        Result->SetStringField(TEXT("componentName"), ComponentName);
        SendInteractableResult(Subsystem, RequestId, RequestingSocket, Blueprint, Result,
                               {TEXT("added interaction component")}, TEXT("Interaction component added"));
        return true;
    }

    if (SubAction != TEXT("configure_interaction_trace"))
    {
        return false;
    }
    UBlueprint* Blueprint = LoadInteractableBlueprint(Subsystem, RequestId, RequestingSocket, Payload,
                                                      TEXT("blueprintPath"), TEXT("interactable"), {});
    if (!Blueprint)
    {
        return true;
    }
    const FString TraceType = GetJsonStringField(Payload, TEXT("traceType"), TEXT("sphere"));
    const double TraceRadius = GetJsonNumberField(Payload, TEXT("traceRadius"), 50.0);
    // Every sphere and box component becomes the trace volume: a sphere of radius traceDistance, a box
    // traceDistance long and traceRadius wide.
    bool bConfigured = false;
    if (USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript)
    {
        for (USCS_Node* Node : SCS->GetAllNodes())
        {
            UObject* Template = Node ? Node->ComponentTemplate : nullptr;
            if (Cast<USphereComponent>(Template) || Cast<UBoxComponent>(Template))
            {
                ConfigureInteractionShape(Template, static_cast<float>(TraceDistance));
                if (UBoxComponent* Box = Cast<UBoxComponent>(Template))
                {
                    Box->SetBoxExtent(FVector(TraceDistance, TraceRadius, TraceRadius));
                }
                bConfigured = true;
            }
        }
    }
    // An SCS with no sphere/box used to report success with a ledger claiming the trace was configured.
    if (!bConfigured)
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("%s has no interaction trace component: expected an SCS node whose class is a SphereComponent or BoxComponent. Run create_interaction_component first, or target an interactable that has one."),
                            *GetJsonStringField(Payload, TEXT("blueprintPath"))),
            TEXT("INVALID_OBJECT_TYPE"));
        return true;
    }

    using EType = EInteractionVarType;
    const int32 NotApplied = ApplyInteractionVars(Blueprint, {
        {TEXT("TraceDistance"), EType::Float, MakeShared<FJsonValueNumber>(TraceDistance)},
        {TEXT("TraceType"), EType::Name, MakeShared<FJsonValueString>(TraceType)}});
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("traceType"), TraceType);
    Result->SetNumberField(TEXT("traceDistance"), TraceDistance);
    Result->SetNumberField(TEXT("traceRadius"), TraceRadius);
    Result->SetBoolField(TEXT("configured"), true);
    Result->SetBoolField(TEXT("propertiesApplied"), NotApplied == 0);
    SendInteractableResult(Subsystem, RequestId, RequestingSocket, Blueprint, Result,
                           {TEXT("configured interaction trace")}, TEXT("Interaction trace configured"));
    return true;
}
}
