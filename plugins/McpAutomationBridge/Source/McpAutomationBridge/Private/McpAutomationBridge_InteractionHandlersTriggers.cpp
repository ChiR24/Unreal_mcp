#include "McpAutomationBridge_InteractionHandlersPrivate.h"

namespace
{
#if WITH_EDITOR
void ConfigureTriggerTemplate(const FString& TriggerShape, USCS_Node*& RootNode, UBlueprint* Blueprint)
{
    if (TriggerShape == TEXT("sphere"))
    {
        RootNode = Blueprint->SimpleConstructionScript->CreateNode(USphereComponent::StaticClass(), TEXT("TriggerVolume"));
        if (USphereComponent* Template = RootNode ? Cast<USphereComponent>(RootNode->ComponentTemplate) : nullptr)
        {
            Template->SetSphereRadius(200.0f);
            Template->SetCollisionProfileName(TEXT("OverlapAll"));
            Template->SetGenerateOverlapEvents(true);
        }
        return;
    }
    if (TriggerShape == TEXT("capsule"))
    {
        RootNode = Blueprint->SimpleConstructionScript->CreateNode(UCapsuleComponent::StaticClass(), TEXT("TriggerVolume"));
        if (UCapsuleComponent* Template = RootNode ? Cast<UCapsuleComponent>(RootNode->ComponentTemplate) : nullptr)
        {
            Template->SetCapsuleSize(50.0f, 100.0f);
            Template->SetCollisionProfileName(TEXT("OverlapAll"));
            Template->SetGenerateOverlapEvents(true);
        }
        return;
    }

    RootNode = Blueprint->SimpleConstructionScript->CreateNode(UBoxComponent::StaticClass(), TEXT("TriggerVolume"));
    if (UBoxComponent* Template = RootNode ? Cast<UBoxComponent>(RootNode->ComponentTemplate) : nullptr)
    {
        Template->SetBoxExtent(FVector(100.0f, 100.0f, 100.0f));
        Template->SetCollisionProfileName(TEXT("OverlapAll"));
        Template->SetGenerateOverlapEvents(true);
    }
}
#endif
}

namespace McpInteractionHandlers
{
bool HandleTriggerAction(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (SubAction == TEXT("create_trigger_actor"))
    {
        const FString Name = GetJsonStringField(Payload, TEXT("name"));
        const FString Folder = GetJsonStringField(Payload, TEXT("folder"), TEXT("/Game/Triggers"));
        const FString TriggerShape = GetJsonStringField(Payload, TEXT("triggerShape"), TEXT("box"));
        if (Name.IsEmpty())
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: name"), TEXT("MISSING_PARAMETER"));
            return true;
        }
#if WITH_EDITOR
        UPackage* Package = CreatePackage(*MakeLegacyPackageName(Folder, Name, TEXT("/Game/Triggers")));
        if (!Package)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_CREATE_FAILED"));
            return true;
        }

        UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
        Factory->ParentClass = AActor::StaticClass();
        UBlueprint* TriggerBP = Cast<UBlueprint>(Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, *Name, RF_Public | RF_Standalone, nullptr, GWarn));
        if (!TriggerBP)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create trigger blueprint"), TEXT("BLUEPRINT_CREATE_FAILED"));
            return true;
        }

        USCS_Node* RootNode = nullptr;
        ConfigureTriggerTemplate(TriggerShape, RootNode, TriggerBP);
        if (RootNode)
        {
            TriggerBP->SimpleConstructionScript->AddNode(RootNode);
        }
        FBlueprintEditorUtils::MarkBlueprintAsModified(TriggerBP);
        McpSafeAssetSave(TriggerBP);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("triggerPath"), TriggerBP->GetPathName());
        Result->SetStringField(TEXT("blueprintPath"), TriggerBP->GetPathName());
        Result->SetStringField(TEXT("triggerShape"), TriggerShape);
        Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Trigger actor created"), Result);
#else
        Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("create_trigger_actor is editor-only"), TEXT("EDITOR_ONLY"));
#endif
        return true;
    }

    if (SubAction != TEXT("configure_trigger_events") &&
        SubAction != TEXT("configure_trigger_filter") &&
        SubAction != TEXT("configure_trigger_response"))
    {
        return false;
    }

    const FString TriggerPath = GetJsonStringField(Payload, TEXT("triggerPath"));
    if ((SubAction == TEXT("configure_trigger_filter") || SubAction == TEXT("configure_trigger_response")) && TriggerPath.IsEmpty())
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: triggerPath"), TEXT("MISSING_PARAMETER"));
        return true;
    }
#if WITH_EDITOR
    FString ResolvedPath;
    FString LoadError;
    UBlueprint* Blueprint = LoadBlueprintAsset(TriggerPath, ResolvedPath, LoadError);
    if (!Blueprint)
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId, LoadError, TEXT("BLUEPRINT_NOT_FOUND"));
        return true;
    }

    TArray<TSharedPtr<FJsonValue>> Added;
    if (SubAction == TEXT("configure_trigger_events"))
    {
        FEdGraphPinType DelegateType;
        DelegateType.PinCategory = UEdGraphSchema_K2::PC_MCDelegate;
        AddBlueprintVariableIfMissing(Blueprint, TEXT("OnTriggerEntered"), DelegateType, &Added);
        AddBlueprintVariableIfMissing(Blueprint, TEXT("OnTriggerExited"), DelegateType, &Added);
        AddBlueprintVariableIfMissing(Blueprint, TEXT("OnTriggerActivated"), DelegateType, &Added);
    }
    else if (SubAction == TEXT("configure_trigger_filter"))
    {
        FEdGraphPinType StringType;
        StringType.PinCategory = UEdGraphSchema_K2::PC_String;
        FEdGraphPinType BoolType;
        BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
        AddBlueprintVariableIfMissing(Blueprint, TEXT("RequiredActorTag"), StringType, &Added);
        AddBlueprintVariableIfMissing(Blueprint, TEXT("bFilterByActorTag"), BoolType, &Added);
    }
    else
    {
        FEdGraphPinType StringType;
        StringType.PinCategory = UEdGraphSchema_K2::PC_String;
        FEdGraphPinType FloatType;
        FloatType.PinCategory = UEdGraphSchema_K2::PC_Real;
        FloatType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
        AddBlueprintVariableIfMissing(Blueprint, TEXT("TriggerResponseType"), StringType, &Added);
        AddBlueprintVariableIfMissing(Blueprint, TEXT("TriggerResponseDelay"), FloatType, &Added);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("configured"), true);
    Result->SetStringField(TEXT("triggerPath"), TriggerPath);
    Result->SetArrayField(SubAction == TEXT("configure_trigger_events") ? TEXT("eventsAdded") : TEXT("variablesAdded"), Added);
    if (SubAction == TEXT("configure_trigger_events"))
    {
        Result->SetNumberField(TEXT("eventCount"), Added.Num());
    }
    if (SubAction == TEXT("configure_trigger_events"))
    {
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    }
    else
    {
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    }
    McpSafeAssetSave(Blueprint);
    const FString Message = SubAction == TEXT("configure_trigger_events") ? TEXT("Trigger events configured") :
        SubAction == TEXT("configure_trigger_filter") ? TEXT("Trigger filter configured") : TEXT("Trigger response configured");
    Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true, Message, Result);
#else
    const FString Message = SubAction == TEXT("configure_trigger_events") ? TEXT("configure_trigger_events is editor-only") :
        SubAction == TEXT("configure_trigger_filter") ? TEXT("configure_trigger_filter is editor-only") : TEXT("configure_trigger_response is editor-only");
    Subsystem->SendAutomationError(RequestingSocket, RequestId, Message, TEXT("EDITOR_ONLY"));
#endif
    return true;
}
}
